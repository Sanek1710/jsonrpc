#include "json-rpc/json-rpc.h"

#include <iostream>
#include <optional>

#include "nlohmann/json.hpp"

void JsonRpcTransport::notify(std::string method,
                              std::optional<json> params) {
  json message = {
      {"jsonrpc", "2.0"},
      {"method", std::move(method)},
  };
  if (params) message.emplace("params", std::move(*params));
  send(std::move(message));
}

void JsonRpcTransport::call(json id, std::string method,
                            std::optional<json> params) {
  json message = {
      {"jsonrpc", "2.0"},
      {"id", std::move(id)},
      {"method", std::move(method)},
  };
  if (params) message.emplace("params", std::move(*params));
  send(std::move(message));
}

void JsonRpcTransport::reply(json id, json result) {
  send(json{
      {"jsonrpc", "2.0"},
      {"id", std::move(id)},
      {"result", std::move(result)},
  });
}

void JsonRpcTransport::error(json id, const JsonRpcError& err) {
  json error = {
      {"code", err.code()},
      {"message", err.message()},
  };
  if (err.data()) error.emplace("data", err.data().value());
  send(json{
      {"jsonrpc", "2.0"},
      {"id", std::move(id)},
      {"error", std::move(error)},
  });
}

std::error_code JsonRpcTransport::loop(JsonRpcHandler& handler) {
  auto trim_buffer = [](std::string& buffer) {
    constexpr size_t json_buffer_hard_limit = 10240;  // 10 Kb
    if (buffer.size() > json_buffer_hard_limit) {
      buffer.resize(json_buffer_hard_limit);
      buffer.shrink_to_fit();
    }
  };

  std::string json_buffer;
  while (true) {
    if (auto err = transport->read(json_buffer)) {
      if (err == std::errc::io_error) return err;
      trim_buffer(json_buffer);
      std::cerr << "transport error: " << err.message() << "\n";
      continue;
    }

    json message;
    try {
      message = json::parse(json_buffer, nullptr, false);
      trim_buffer(json_buffer);
    } catch (const json::parse_error& err) {
      trim_buffer(json_buffer);
      std::cerr << "parse error: " << err.what() << "\n";
      continue;
    }
    if (!handle_message(std::move(message), handler)) {
      return std::error_code{};
    }
  }
  return std::make_error_code(std::errc::io_error);
}

namespace {

void log_error(std::string_view err_msg, const json& object) {
  std::cerr << "jsonrpc: " << err_msg << ": " << object.dump() << "\n";
}

std::optional<json> move_json_to_opt(json& j, const std::string& key) {
  auto it = j.find(key);
  if (it != j.end()) {
    return std::move(*it);
  }
  return std::nullopt;
}

}  // namespace

void JsonRpcTransport::send(const json& message) {
  transport->write(message.dump());
}

bool JsonRpcTransport::handle_message(json message, JsonRpcHandler& handler) {
  if (!message.is_object() || message.value("jsonrpc", "") != "2.0") {
    std::string err_msg = "not a jsonrpc message";
    log_error(err_msg, message);
    error(message.value("id", json{nullptr}),
          JsonRpcRuntimeError(JsonRpcErrc::InvalidRequest, std::move(err_msg)));
    return true;
  }

  if (auto method_it = message.find("method"); method_it != message.end()) {
    if (!method_it->is_string()) {
      std::string err_msg = "'method' is not a string";
      log_error(err_msg, message);
      error(
          message.value("id", json{nullptr}),
          JsonRpcRuntimeError(JsonRpcErrc::InvalidRequest, std::move(err_msg)));
      return true;
    }

    if (auto id_it = message.find("id"); id_it != message.end()) {
      return handler.on_call(std::move(*id_it),
                             std::move(method_it->get_ref<std::string&>()),
                             move_json_to_opt(message, "params"));
    }
    return handler.on_notify(std::move(method_it->get_ref<std::string&>()),
                             move_json_to_opt(message, "params"));
  }

  auto id_it = message.find("id");
  if (id_it == message.end()) {
    std::string err_msg = "message has no 'id'";
    log_error(err_msg, message);
    error(nullptr,
          JsonRpcRuntimeError{JsonRpcErrc::InvalidRequest, std::move(err_msg)});
    return true;
  }

  if (auto result_it = message.find("result"); result_it != message.end()) {
    return handler.on_reply(std::move(*id_it), std::move(*result_it));
  }
  if (auto error_it = message.find("error"); error_it != message.end()) {
    if (!error_it->is_object()) {
      log_error("invalid error message format: message is not an object",
                message);

      return true;
    }
    auto err_code_it = error_it->find("code");
    if (err_code_it == error_it->end() || !err_code_it->is_number_integer()) {
      log_error("invalid error message format: error 'code' is not an integer",
                message);
      return true;
    }
    auto err_msg_it = error_it->find("message");
    if (err_msg_it == error_it->end() || !err_msg_it->is_string()) {
      log_error(
          "invalid error message format: error 'message' is not an string",
          message);
      return true;
    }
    return handler.on_error(std::move(*id_it), err_code_it->get<int>(),
                            std::move(err_msg_it->get_ref<std::string&>()),
                            move_json_to_opt(*error_it, "data"));
  }

  std::string err_msg = "none of 'method', 'result' or 'error' provided";
  log_error(err_msg, message);
  error(message.value("id", json{nullptr}),
        JsonRpcRuntimeError{JsonRpcErrc::InvalidRequest, std::move(err_msg)});
  return true;
}

std::string JsonRpcError::message() const {
  switch (_code) {
    case JsonRpcErrc::ParseError:
      return "parse error";
    case JsonRpcErrc::InvalidRequest:
      return "invalid request";
    case JsonRpcErrc::MethodNotFound:
      return "method not found";
    case JsonRpcErrc::InvalidParams:
      return "invalid params";
    case JsonRpcErrc::InternalError:
      return "internal error";
    default:
      if (-32000 <= _code && _code <= -32099) return "server error";
      return "unknown error";
  }
}

const std::optional<json>& JsonRpcError::data() const {
  static const std::optional<json> no_data = std::nullopt;
  return no_data;
}
