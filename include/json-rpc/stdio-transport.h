#pragma once

#include <iostream>
#include <mutex>
#include <string>

#include "json-rpc/transport.h"

class StdioTransport : public Transport {
 public:
  StdioTransport(std::istream& ins, std::ostream& outs)
      : ins(ins), outs(outs) {}

  std::error_code read(std::string& content) override {
    if (!std::getline(ins, content)) {
      return std::make_error_code(std::errc::io_error);
    }
    if (!content.empty() && content.back() == '\r') content.pop_back();
    if (content.empty()) return std::make_error_code(std::errc::no_message);
    return {};
  }

  void write(std::string_view content) override {
    const std::lock_guard<std::mutex> lock(mtx_);
    outs << content << "\n" << std::flush;
  }

 private:
  std::istream& ins;
  std::ostream& outs;
  mutable std::mutex mtx_;
};
