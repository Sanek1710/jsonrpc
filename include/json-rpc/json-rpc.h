#pragma once

#include <optional>
#include <utility>

#include "nlohmann/json_fwd.hpp"
#include "transport.h"

using json = nlohmann::json;

namespace JsonRpcErrc {

constexpr int ParseError = -32700;
constexpr int InvalidRequest = -32600;
constexpr int MethodNotFound = -32601;
constexpr int InvalidParams = -32602;
constexpr int InternalError = -32603;

};  // namespace JsonRpcErrc

class JsonRpcError {
 public:
  static constexpr int Success = 0;

  JsonRpcError(int code) : _code(code) {}
  int code() const { return _code; }

  virtual std::string message() const;
  virtual const std::optional<json>& data() const;

  operator bool() const { return _code != Success; }

 private:
  int _code;
};

class JsonRpcRuntimeError : public JsonRpcError {
 public:
  JsonRpcRuntimeError(int code, std::string message)
      : JsonRpcError{code}, _message(std::move(message)) {}

  virtual std::string message() const override { return _message; };

 private:
  std::string _message;
};

class JsonRpcHandler {
 public:
  virtual ~JsonRpcHandler() = default;
  virtual bool on_notify(std::string method,
                         std::optional<json> params) = 0;
  virtual bool on_call(json id, std::string method,
                       std::optional<json> params) = 0;
  virtual bool on_reply(json id, json result) = 0;
  virtual bool on_error(json id, int code, std::string message,
                        std::optional<json> data) = 0;


};

class JsonRpcTransport {
 public:
  JsonRpcTransport(std::unique_ptr<Transport> transport)
      : transport(std::move(transport)) {}

  void notify(std::string method, std::optional<json> params);
  void call(json id, std::string method, std::optional<json> params);
  void reply(json id, json result);
  void error(json id, const JsonRpcError& err);

  std::error_code loop(JsonRpcHandler& handler);

 private:
  std::unique_ptr<Transport> transport;

  bool handle_message(json message, JsonRpcHandler& handler);
  void send(const json& message);
};
