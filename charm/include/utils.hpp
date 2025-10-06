#pragma once
#include <cstdio>
#include <string>

namespace charm::utils {

inline const char *cstr(const std::string &v) { return v.c_str(); }
template <typename T> inline T cstr(T v) { return v; }

template <typename... Args>
inline std::string sformat(const std::string &fmt, Args... args) {
  int length =
      std::snprintf(nullptr, 0, fmt.c_str(), cstr(std::forward<Args>(args))...);

  std::string buffer;
  buffer.resize(length + 1);
  std::snprintf(buffer.data(), buffer.size(), fmt.c_str(),
                cstr(std::forward<Args>(args))...);

  return buffer;
}

} // namespace charm::utils
