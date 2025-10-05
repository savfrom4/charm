#pragma once
#include <cstdio>
#include <string>

template <typename... Args>
inline std::string sformat(const std::string &fmt, Args... args) {
  int length = std::snprintf(nullptr, 0, fmt.c_str(), args...);

  std::string buffer;
  buffer.resize(length);
  std::snprintf(buffer.c_str(), buffer.size(), fmt.c_str(), args...);

  return buffer;
}
