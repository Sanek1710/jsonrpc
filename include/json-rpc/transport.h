#pragma once

#include <string>
#include <system_error>

class Transport {
 public:
  virtual std::error_code read(std::string& content) = 0;
  virtual void write(std::string_view content) = 0;
  virtual ~Transport() {};
};
