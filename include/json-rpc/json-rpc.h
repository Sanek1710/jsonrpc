#pragma once

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

class JsonRpcHandler {
 public:
  virtual ~JsonRpcHandler() = default;
  virtual bool on_notify(std::string_view method,
                         std::optional<json> params) = 0;
  virtual bool on_call(json id, std::string_view method,
                       std::optional<json> params) = 0;
  virtual bool on_reply(json id, json result) = 0;
  virtual bool on_error(json id, int code, std::string message,
                        std::optional<json> data) = 0;
};

class JsonRpcTransport {
 public:
  JsonRpcTransport(std::unique_ptr<Transport> transport)
      : transport(std::move(transport)) {}

  void notify(std::string_view method, std::optional<json> params);
  void call(json id, std::string_view method, std::optional<json> params);
  void reply(json id, json result);
  void error(json id, int code, std::string message, std::optional<json> data);

  std::error_code loop(JsonRpcHandler& handler);

 private:
  std::unique_ptr<Transport> transport;

  bool handle_message(json message, JsonRpcHandler& handler);
  void send(const json& message);
};
