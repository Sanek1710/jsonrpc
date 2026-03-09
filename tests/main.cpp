#include <cassert>
#include <iostream>
#include <system_error>

#include "json-rpc/json-rpc.h"
#include "json-rpc/lsp-transport.h"
#include "json-rpc/stdio-transport.h"
#include "nlohmann/json.hpp"

#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define BLUE "\x1b[34m"
#define RESET "\x1b[0m"

using json = nlohmann::json;

class MockHandler : public JsonRpcHandler {
 public:
  bool on_notify(std::string_view m, std::optional<json> p) override {
    return true;
  }
  bool on_call(json id, std::string_view m, std::optional<json> p) override {
    return true;
  }
  bool on_reply(json id, json r) override { return true; }
  bool on_error(json id, int c, std::string msg,
                std::optional<json> d) override {
    return true;
  }
};

void test_call_serialization() {
  std::istringstream istr{};
  std::ostringstream ostr;
  auto trans_ptr = std::make_unique<StdioTransport>(istr, ostr);
  JsonRpcTransport rpc(std::move(trans_ptr));

  rpc.call(1, "test/method", json{{"key", "value"}});

  std::string sent_data = ostr.str();
  if (!sent_data.empty() && sent_data.back() == '\n') sent_data.pop_back();
  std::cerr << BLUE "sent: " RESET "`" << sent_data << "`\n";
  auto j = json::parse(sent_data);
  assert(j["jsonrpc"] == "2.0");
  assert(j["method"] == "test/method");
  assert(j["id"] == 1);
  assert(j["params"]["key"] == "value");

  std::cout << GREEN "[OK] Call serialization\n" RESET;
}

void test_notify_serialization() {
  std::istringstream istr{};
  std::ostringstream ostr;
  auto trans_ptr = std::make_unique<StdioTransport>(istr, ostr);
  JsonRpcTransport rpc(std::move(trans_ptr));

  rpc.notify("initialized", std::nullopt);

  std::string sent_data = ostr.str();
  if (!sent_data.empty() && sent_data.back() == '\n') sent_data.pop_back();
  std::cerr << BLUE "sent: " RESET "`" << sent_data << "`\n";
  auto j = json::parse(sent_data);
  assert(j["method"] == "initialized");
  assert(!j.contains("id"));

  std::cout << GREEN "[OK] Notify serialization\n" RESET;
}

void test_error_serialization() {
  std::istringstream istr{};
  std::ostringstream ostr;
  auto trans_ptr = std::make_unique<StdioTransport>(istr, ostr);
  JsonRpcTransport rpc(std::move(trans_ptr));

  rpc.error(123, JsonRpcErrc::InvalidParams, "Invalid params",
            json{{"detail", "missing field"}});

  std::string sent_data = ostr.str();
  if (!sent_data.empty() && sent_data.back() == '\n') sent_data.pop_back();
  std::cerr << BLUE "sent: " RESET "`" << sent_data << "`\n";
  auto j = json::parse(sent_data);
  assert(j["id"] == 123);
  assert(j["error"]["code"] == -32602);
  assert(j["error"]["message"] == "Invalid params");
  assert(j["error"]["data"]["detail"] == "missing field");

  std::cout << GREEN "[OK] Error serialization\n" RESET;
}

void test_lsp_loop() {
  std::istringstream istr{
      "Content-Length: 111\r\n\r\n"
      R"json({"jsonrpc":"2.0","error":{"code":-32602,"data":{"detail":"missing field"},"message":"Invalid params"},"id":123})json"
      "some data\r\n"
      "\r\n"
      "\r\n"
      "\r\n"
      "some more data\r\n"
      "Content-Length: 72\r\n\r\n"
      R"json({"jsonrpc":"2.0","id":1,"method":"test/method","params":{"key":"value"}})json"
      "Content-Length: 65\r\n\r\n"
      R"json({"jsonrpc":"2.0","method":"test/method","params":{"key":"value"}})json"
      "some data"
      ""
      ""
      ""
      "some more data"
      "Content-Length: 70\r\n\r\n"
      R"json({"jsonrpc":"2.0","id":1,"method":"not/valid","params":{"key":"value"}})json"};
  std::ostringstream ostr;

  auto trans_ptr = std::make_unique<LspStdioTransport>(istr, ostr);
  JsonRpcTransport rpc{std::move(trans_ptr)};

  class MockHandler : public JsonRpcHandler {
   public:
    size_t n_calls = 0;
    size_t n_notifications = 0;
    size_t n_errors = 0;

    bool on_notify(std::string_view m, std::optional<json> p) override {
      n_notifications++;
      std::cerr << BLUE "handled notification: " RESET "`" << m << "`\n";
      assert(m == "test/method");
      assert(p->value("key", "") == "value");
      return true;
    }
    bool on_call(json id, std::string_view m, std::optional<json> p) override {
      n_calls++;
      std::cerr << BLUE "handled call: " RESET "`" << m << "`\n";
      assert(id == 1);
      assert(m == "test/method");
      assert(p->value("key", "") == "value");
      return true;
    }
    bool on_reply(json id, json r) override { return true; }
    bool on_error(json id, int c, std::string msg,
                  std::optional<json> d) override {
      n_errors++;
      std::cerr << BLUE "handled error: " RESET "`" << msg << "`\n";
      assert(c == -32602);
      assert(msg == "Invalid params");
      assert(d->value("detail", "") == "missing field");
      return true;
    }
  } hndl;

  auto err = rpc.loop(hndl);
  assert(err == std::errc::io_error);

  assert(hndl.n_calls == 1);
  assert(hndl.n_errors == 1);
  assert(hndl.n_notifications == 1);

  std::cout << GREEN "[OK] loop\n" RESET;
}

int main() {
  try {
    test_call_serialization();
    test_notify_serialization();
    test_error_serialization();
    test_lsp_loop();
    std::cout << GREEN "\nALL TESTS PASSED\n" RESET;
  } catch (const std::exception& e) {
    std::cerr << RED "\nTEST FAILED: " RESET << e.what() << "\n";
    return 1;
  }
  return 0;
}
