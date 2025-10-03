#include "liblayer/debug.hpp"
#include "liblayer/execution_state.hpp"
#include <arpa/inet.h>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdexcept>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// NOTE: excludes 1 byte header
const std::array<size_t, (int)layer::DebugCommand::COUNT> COMMAND_SIZE_TABLE = {
    0,
    sizeof(std::uint32_t), // BREAK (32-bit imm address)
    0,                     // STEP
    0,                     // SKIP
    sizeof(std::uint8_t),  // PAUSE_MODE (8-bit boolean)
    sizeof(std::uint8_t),  // PRINT_REGISTER (8-bit register index)
    sizeof(std::uint32_t), // PRINT_AT_ADDRESS (32-bit imm address)
    sizeof(std::uint32_t), // DUMP (32-bit imm filename length + n bytes of
                           // string)
    sizeof(std::uint32_t), // RESTORE (32-bit imm filename length + n bytes of
                           // string)
};

namespace layer {

Debugee::Debugee(ExecutionState &ps) {
  _socket = socket(AF_INET, SOCK_STREAM, 0);

  if (!_socket) {
    throw std::runtime_error("ExecutionDebugee ctor: failed to create socket.");
  }

  struct sockaddr_in address = {
      .sin_family = AF_INET,
      .sin_port = htons(LAYER_DEBUG_PORT),
      .sin_addr = {.s_addr = INADDR_ANY},
      .sin_zero = {0},
  };

  if (bind(_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
    throw std::runtime_error("ExecutionDebugee ctor: failed to bind socket.");
  }

  std::cout << "Waitng for debugger on port " << LAYER_DEBUG_PORT << "..."
            << std::endl;

  if (listen(_socket, 1) < 0) {
    throw std::runtime_error(
        "ExecutionDebugee ctor: failed to listen on socket.");
  }

  socklen_t address_len;
  if (!(_connection =
            accept(_socket, (struct sockaddr *)&address, &address_len))) {
    throw std::runtime_error(
        "ExecutionDebugee ctor: failed to accept the connection.");
  }

  // disable nagle's algorithm
  int one = 1;
  if (setsockopt(_connection, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) <
      0) {
    throw std::runtime_error("ExecutionDebugee ctor: failed to set NODELAY.");
  }

  std::cout << "Connection established." << std::endl;

  while (_flags & PAUSED)
    process(ps, 1000); // wait for continue
}

Debugee::~Debugee() {
  if (_socket) {
    close(_socket);
  }

  if (_connection) {
    close(_connection);
  }
}

void Debugee::step(ExecutionState &ps) {
  if (_flags & STEP) {
    _flags &= ~STEP; // clear flag
    send_paused(ps);
  }

  while (_flags & PAUSED)
    process(ps, 1000); // wait for continue
}

void Debugee::skip(ExecutionState &ps, const std::string &info) {
  bool is_breakpoint = _breakpoints.count(ps.r[PC] - 8);

  if (is_breakpoint || _flags & SKIP) {
    _flags &= ~SKIP; // clear flag

    if (is_breakpoint) {
      send_fmt("Breakpoint: %s", info);
    } else {
      send_fmt("%s", info);
    }

    send_paused(ps);
  }

  while (_flags & PAUSED)
    process(ps, 1000); // wait for continue
}

void Debugee::send_paused(ExecutionState &ps) {
  _flags |= PAUSED; // pause

  std::uint32_t length = 0; // sending length 0 is pause request
  write(_connection, &length, sizeof(length));
}

void Debugee::send_raw() {
  auto temp_buffer_ptr = _temp_buffer.data();

  std::uint32_t length = std::strlen(reinterpret_cast<char *>(temp_buffer_ptr));
  length = htonl(length);

  std::memmove(temp_buffer_ptr + sizeof(length), temp_buffer_ptr, length);
  std::memcpy(temp_buffer_ptr, &length, sizeof(length));

  write(_connection, temp_buffer_ptr, length + sizeof(length));
}

bool Debugee::poll(int timeout) {
  struct pollfd fd = {
      .fd = _connection,
      .events = POLLIN,
      .revents = 0,
  };

  int result = ::poll(&fd, 1, timeout);
  if (result == 0) {
    return false; /* nothing, still wait */
  }

  if (!result) {
    throw std::runtime_error(
        "Debugee::poll: invalid file descriptor (connection is lost...?).");
  }

  return (fd.revents & POLLIN);
}

void Debugee::process(ExecutionState &ps, int timeout) {
  auto temp_buffer_ptr = _temp_buffer.data();

  while (poll(timeout)) {
    ssize_t bytes_read =
        ::read(_connection, temp_buffer_ptr, sizeof(_temp_buffer));

    if (!bytes_read) {
      throw std::runtime_error("Debugee::poll: connection is lost.");
    }

    _accum_buffer.insert(_accum_buffer.end(), temp_buffer_ptr,
                         temp_buffer_ptr + bytes_read);
  }

  // try to read as much as we can
  while (1) {
    if (!_accum_buffer.size()) {
      return;
    }

    // read command type
    if (_command == DebugCommand::NONE) {
      std::memcpy(&_command, _accum_buffer.data(), sizeof(_command));
      _accum_buffer.erase(_accum_buffer.begin());
    }

    // not enough data
    const auto command_size = COMMAND_SIZE_TABLE[(int)_command];
    if (_accum_buffer.size() < command_size) {
      return;
    }

    process_command(ps);

    _accum_buffer.erase(_accum_buffer.begin(),
                        _accum_buffer.begin() + command_size);
    _command = DebugCommand::NONE;
  }
}

void Debugee::process_command(ExecutionState &ps) {
  switch (_command) {
  case DebugCommand::BREAK: {
    std::uint32_t value;
    std::memcpy(&value, _accum_buffer.data(), sizeof(value));

    if (_breakpoints.count(value)) {
      _breakpoints.erase(value);
      send_fmt("Breakpoint at 0x%X removed.", value);
      break;
    }

    _breakpoints.emplace(value);
    send_fmt("Breakpoint at 0x%X set.", value);
    break;
  }

  case DebugCommand::STEP: {
    _flags |= STEP;
    break;
  }

  case layer::DebugCommand::SKIP: {
    _flags |= SKIP;
    break;
  }

  case DebugCommand::PAUSE_MODE: {
    std::uint8_t value;
    std::memcpy(&value, _accum_buffer.data(), sizeof(value));

    if (value) {
      _flags |= PAUSED;
    } else {
      _flags &= ~PAUSED;
    }
    break;
  }

  case DebugCommand::PRINT_REGISTER: {
    std::uint8_t value;
    std::memcpy(&value, _accum_buffer.data(), sizeof(value));

    send_fmt("r%d=0x%X (%d)", value, ps.r[value], ps.r[value]);
    break;
  }

  case DebugCommand::PRINT_AT_ADDRESS: {
    std::uint32_t value;
    std::memcpy(&value, _accum_buffer.data(), sizeof(value));

    const std::uint8_t *ptr =
        reinterpret_cast<const std::uint8_t *>(ps.address_resolve(value));
    send_fmt("0x%X=0x%X (%d)", value, *ptr, *ptr);
    break;
  }

  case DebugCommand::DUMP:
  case DebugCommand::RESTORE:
    break;

  default:
    throw std::runtime_error("Debugee::process_command: invalid command type.");
  }
}

} // namespace layer
