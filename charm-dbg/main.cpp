#include "liblayer/debug.hpp"
#include <array>
#include <chrono>
#include <cstring>
#include <future>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdexcept>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <type_traits>
#include <unistd.h>
#include <vector>

const std::string VERSION = "1.0.0";

constexpr std::size_t hasher(const char *str, std::size_t hash = 5381) {
  return *str ? hasher(str + 1, (hash * 33) ^ static_cast<unsigned char>(*str))
              : hash;
}

template <typename T>
inline void buffer_write(std::array<char, 512> &buffer, std::uintptr_t &offset,
                         T value) {
  static_assert(std::is_enum_v<T> || std::is_integral_v<T>,
                "buffer_write: T must be an enum or an integer.");

  if constexpr (sizeof(value) == sizeof(std::uint16_t)) {
    value = htons(value);
  } else if constexpr (sizeof(value) == sizeof(std::uint32_t)) {
    value = htonl(value);
  }

  std::memcpy(buffer.data() + offset, &value, sizeof(value));
  offset += sizeof(value);
}

void help_show();
void debugger_start(struct addrinfo *info);
void debugger_execute_command(int connection, const std::string &full_command,
                              bool &paused, std::array<char, 512> &temp_buffer);
bool debugger_network_process(int connection,
                              std::array<char, 512> &temp_buffer,
                              std::vector<char> &accum_buffer,
                              std::uint32_t &length, bool &paused, int timeout);
bool debugger_network_poll(int connection, int timeout);

int main(int argc, char **argv) {
  const std::string full_address = argc > 1 ? argv[1] : "127.0.0.1:6969";
  const auto colon_location = full_address.find(':');

  if (colon_location == std::string::npos) {
    help_show();
    return 1;
  }

  const std::string address = full_address.substr(0, colon_location);
  const std::string port = full_address.substr(colon_location + 1);

  std::cout << "> Connecting to \"" << address << "\", port " << port << " ..."
            << std::endl;

  struct addrinfo hints, *info = nullptr;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(address.c_str(), port.c_str(), &hints, &info) != 0) {
    throw std::runtime_error("Failed to resolve specified address.");
  }

  if (!info) {
    throw std::runtime_error("Failed to resolve specified address.");
  }

  debugger_start(info);
  freeaddrinfo(info);
  return 0;
}

void help_show() {
  std::cout << "charm-dbg v" << VERSION << " — Recompiler output debugger."
            << std::endl;
  std::cout << "Licensed under the MIT License © 2025 sstochi and contributors."
            << std::endl
            << std::endl;

  std::cout << "Usage:" << std::endl
            << "\tcharm-dbg [ADDRESS:PORT]" << std::endl
            << std::endl;

  std::cout << "Examples:" << std::endl
            << "\tcharm-dbg // connect to localhost, default port 6969"
            << std::endl
            << "\tcharm-dbg 127.0.0.1:6969 // connect to specified address"
            << std::endl;
}

void debugger_start(struct addrinfo *info) {
  int connection = socket(AF_INET, SOCK_STREAM, 0);
  if (connection < 0) {
    throw std::runtime_error("debugger_start: failed to create socket.");
  }

  if (connect(connection, info->ai_addr, info->ai_addrlen) < 0) {
    throw std::runtime_error("debugger_start: failed to connect.");
  }

  // disable nagle's algorithm
  int one = 1;
  if (setsockopt(connection, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) < 0) {
    throw std::runtime_error("ExecutionDebugee ctor: failed to set NODELAY.");
  }

  std::cout << "> Connection established! Type help(h) for command list."
            << std::endl;

  std::array<char, 512> temp_buffer = {0};
  std::vector<char> accum_buffer;
  std::uint32_t length = 0;
  bool paused = true;

  while (1) {
    // when paused, no control is given to user
    if (!paused) {
      debugger_network_process(connection, temp_buffer, accum_buffer, length,
                               paused, 30);
      continue;
    }

    std::future<std::string> future = std::async(std::launch::async, []() {
      std::string line;
      std::getline(std::cin, line);
      return line;
    });

    std::cout << "> ";
    std::cout.flush();

    // while we wait for input, poll socket
    while (future.wait_for(std::chrono::milliseconds(30)) !=
           std::future_status::ready) {
      if (debugger_network_process(connection, temp_buffer, accum_buffer,
                                   length, paused, 0)) {
        std::cout << "> ";
        std::cout.flush();
      }
    }

    // execute command
    debugger_execute_command(connection, future.get(), paused, temp_buffer);
  }

  ::close(connection);
}

void debugger_execute_command(int connection, const std::string &full_command,
                              bool &paused,
                              std::array<char, 512> &temp_buffer) {
  std::uintptr_t buffer_offset = 0;
  const auto split_location = full_command.find(' ');
  const auto command = full_command.substr(0, split_location);

  std::string arg;
  if (split_location != std::string::npos) {
    arg = full_command.substr(split_location + 1);
  }

  switch (hasher(command.c_str())) {
  case hasher("h"):
  case hasher("help"): {
    std::cout << "break(b) <address>" << std::endl;
    std::cout << "print(p) <register/address>" << std::endl;
    std::cout << "continue(c)" << std::endl;
    std::cout << "next(n)" << std::endl;
    std::cout << "skip(s)" << std::endl;
    std::cout << "exit/quit(e/q)" << std::endl;
    break;
  }

  case hasher("b"):
  case hasher("break"): {
    buffer_write(temp_buffer, buffer_offset, layer::DebugCommand::BREAK);
    buffer_write<std::uint32_t>(temp_buffer, buffer_offset,
                                std::stoul(arg, 0, 0));
    break;
  }

  case hasher("p"):
  case hasher("print"): {
    layer::DebugCommand type = layer::DebugCommand::PRINT_REGISTER;
    std::uint32_t value = 0;

    switch (hasher(arg.c_str())) {
    case hasher("r0"):
      value = 0;
      break;

    case hasher("r1"):
      value = 1;
      break;

    case hasher("r2"):
      value = 2;
      break;

    case hasher("r3"):
      value = 3;
      break;

    case hasher("r4"):
      value = 4;
      break;

    case hasher("r5"):
      value = 5;
      break;

    case hasher("r6"):
      value = 6;
      break;

    case hasher("r7"):
      value = 7;
      break;

    case hasher("r8"):
      value = 8;
      break;

    case hasher("r9"):
      value = 9;
      break;

    case hasher("r10"):
      value = 10;
      break;

    case hasher("r11"):
      value = 11;
      break;

    case hasher("r12"):
    case hasher("ip"): {
      value = 12;
      break;
    }

    case hasher("r13"):
    case hasher("sp"): {
      value = 13;
      break;
    }

    case hasher("r14"):
    case hasher("lr"): {
      value = 14;
      break;
    }

    case hasher("r15"):
    case hasher("pc"): {
      value = 15;
      break;
    }

    // address it is then
    default: {
      type = layer::DebugCommand::PRINT_AT_ADDRESS;
      value = std::stoul(arg, 0, 0);
      break;
    }
    }

    buffer_write(temp_buffer, buffer_offset, type);
    buffer_write(temp_buffer, buffer_offset, value);
    break;
  }

  case hasher("c"):
  case hasher("continue"): {
    buffer_write(temp_buffer, buffer_offset, layer::DebugCommand::PAUSE_MODE);
    buffer_write<std::uint8_t>(temp_buffer, buffer_offset, false);
    paused = false;
    break;
  }

  case hasher("n"):
  case hasher("next"): {
    buffer_write(temp_buffer, buffer_offset, layer::DebugCommand::NEXT);
    paused = false;
    break;
  }

  case hasher("s"):
  case hasher("skip"): {
    buffer_write(temp_buffer, buffer_offset, layer::DebugCommand::SKIP);
    paused = false;
    break;
  }

  case hasher("e"):
  case hasher("q"):
  case hasher("exit"):
  case hasher("quit"): {
    exit(0);
    break;
  }

  default: {
    std::cout << "> Unknown command: \"" << command << "\"!" << std::endl;
    return;
  }
  }

  while (buffer_offset > 0) {
    ssize_t bytes_written =
        write(connection, temp_buffer.data(), buffer_offset);

    if (!bytes_written) {
      throw std::runtime_error("debugger_execute_command: connection is lost.");
    }

    buffer_offset -= bytes_written;
  }
}

bool debugger_network_process(int connection,
                              std::array<char, 512> &temp_buffer,
                              std::vector<char> &accum_buffer,
                              std::uint32_t &length, bool &paused,
                              int timeout) {
  auto temp_buffer_ptr = temp_buffer.data();

  while (debugger_network_poll(connection, timeout)) {
    ssize_t bytes_read = read(connection, temp_buffer_ptr, sizeof(temp_buffer));

    if (!bytes_read) {
      throw std::runtime_error("debugger_network_process: connection is lost.");
    }

    accum_buffer.insert(accum_buffer.end(), temp_buffer_ptr,
                        temp_buffer_ptr + bytes_read);
  }

  // try to read as much as we can
  bool result = false;
  while (1) {
    if (sizeof(std::uint32_t) > accum_buffer.size()) {
      break;
    }

    if (!length) {
      std::memcpy(&length, accum_buffer.data(), sizeof(length));
      accum_buffer.erase(accum_buffer.begin(),
                         accum_buffer.begin() + sizeof(length));

      // length == 0: pause request
      if (!length) {
        paused = true;
        result = true;
        break;
      }

      length = ntohl(length);
    }

    if (length > accum_buffer.size()) {
      break;
    }

    // read buffer
    std::string buffer;
    buffer.resize(length);
    std::memcpy(buffer.data(), accum_buffer.data(), length);
    accum_buffer.erase(accum_buffer.begin(), accum_buffer.begin() + length);

    // reset length for later read
    length = 0;
    result = true;

    std::cout << buffer << std::endl;
    std::cout.flush();
  }

  return result;
}

bool debugger_network_poll(int connection, int timeout) {
  struct pollfd fd = {
      .fd = connection,
      .events = POLLIN,
      .revents = 0,
  };

  int result = ::poll(&fd, 1, timeout);
  if (result == 0) {
    return false; /* nothing, still wait */
  }

  if (!result) {
    throw std::runtime_error("debugger_network_poll: invalid file descriptor "
                             "(connection is lost...?).");
  }

  return (fd.revents & POLLIN);
}
