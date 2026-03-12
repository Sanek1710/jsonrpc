#pragma once

#include <charconv>
#include <cstring>
#include <iostream>
#include <mutex>

#include "json-rpc/transport.h"

class LspStdioTransport : public Transport {
  static constexpr std::string_view content_length_header = "Content-Length: ";

 public:
  LspStdioTransport(std::istream& ins, std::ostream& outs, bool newline = false)
      : ins(ins), outs(outs), ending(newline ? "\n" : "") {}

  std::error_code read(std::string& content) override {
    if (ins.eof()) return std::make_error_code(std::errc::io_error);
    size_t content_length = 0;
    if (auto ec = read_header(ins, content_length)) return ec;
    content.resize(content_length);
    ins.read(content.data(), content_length);
    return std::error_code{};
  }

  void write(std::string_view content) override {
    const std::lock_guard lock{mtx};
    outs << content_length_header << (content.length() + ending.size())
         << "\r\n\r\n"
         << content << ending;
    outs.flush();
  }

 private:
  std::istream& ins;
  std::ostream& outs;
  std::string_view ending = "";
  mutable std::mutex mtx;

  std::error_code read_header(std::istream& is, size_t& content_length) {
    const size_t header_size_limit = 128;
    char line[header_size_limit];
    bool found = false;

    while (is.getline(line, header_size_limit)) {
      const size_t n_read = static_cast<size_t>(is.gcount());
      if (n_read <= 2 && (line[0] == '\r' || line[0] == '\0')) break;

      if (std::strncmp(line, content_length_header.data(),
                       content_length_header.length()) == 0) {
        auto result = std::from_chars(line + content_length_header.length(),
                                      line + n_read, content_length);
        if (result.ec != std::errc{}) return std::make_error_code(result.ec);
        found = (result.ptr != line + content_length_header.length());
      }
    }

    if (is.fail() && !is.eof()) {
      is.clear();
      is.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      return std::make_error_code(std::errc::message_size);
    }

    if (!found) return std::make_error_code(std::errc::no_message);
    return std::error_code{};
  }
};
