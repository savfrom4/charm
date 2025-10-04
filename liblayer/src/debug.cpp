#include "liblayer/debug.hpp"
#include "liblayer/execution_state.hpp"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdexcept>
#include <string>
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
    sizeof(std::uint32_t), // PRINT_REGISTER (8-bit register index)
    sizeof(std::uint32_t), // PRINT_AT_ADDRESS (32-bit imm address)
    sizeof(std::uint32_t), // DUMP (32-bit imm filename length + n bytes of
                           // string)
    sizeof(std::uint32_t), // RESTORE (32-bit imm filename length + n bytes of
                           // string)
};

namespace layer {

Debugee::Debugee(ExecutionState &_ps) : _ps(_ps) {
  _socket = socket(AF_INET, SOCK_STREAM, 0);
  if (_socket < 0) {
    throw std::runtime_error("ExecutionDebugee ctor: failed to create socket.");
  }

  struct sockaddr_in address = {
      .sin_family = AF_INET,
      .sin_port = htons(LAYER_DEBUG_PORT),
      .sin_addr = {.s_addr = INADDR_ANY},
      .sin_zero = {0},
  };

  int one = 1;
  if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
    throw std::runtime_error("ExecutionDebugee ctor: failed to set REUSEADDR.");
  }

  if (setsockopt(_socket, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one)) < 0) {
    throw std::runtime_error(
        "ExecutionDebugee ctor: failed to set SO_REUSEPORT.");
  }

  if (bind(_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
    throw std::runtime_error("ExecutionDebugee ctor: failed to bind socket.");
  }

  std::cout << "> Waitng for debugger on port " << LAYER_DEBUG_PORT << "..."
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
  if (setsockopt(_connection, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) <
      0) {
    throw std::runtime_error("ExecutionDebugee ctor: failed to set NODELAY.");
  }

  std::cout << "> Connection established!" << std::endl;

  stall();
  send_format("> Executing...");
}

Debugee::~Debugee() {
  send_format("> Reached program's end. Continuing will close the connection.");
  send_paused();
  stall();

  if (_socket >= 0) {
    close(_socket);
  }

  if (_connection >= 0) {
    close(_connection);
  }
}

void Debugee::next() {
  if (flags & NEXT) {
    flags &= ~NEXT; // clear flag

    send_message();
    send_paused();
  }

  stall();
}

void Debugee::skip() {
  bool is_breakpoint = _breakpoints.count(_ps.r[PC] - 8);

  if (is_breakpoint || flags & SKIP) {
    flags &= ~SKIP; // clear flag

    send_message();

    if (is_breakpoint) {
      send_format("Breakpoint hit.");
    }

    send_paused();
  } else if (flags & NEXT) {
    send_message();
    send_paused();
  }

  stall();
}

void Debugee::send_paused() {
  flags |= PAUSED; // pause

  std::uint32_t length = 0; // sending length 0 is pause request
  write(_connection, &length, sizeof(length));
}

void Debugee::send_message() {
  auto temp_buffer_ptr = _temp_buffer.data();

  std::uint32_t length = std::strlen(temp_buffer_ptr);
  std::memmove(temp_buffer_ptr + sizeof(length), temp_buffer_ptr, length + 1);

  length = htonl(length);
  std::memcpy(temp_buffer_ptr, &length, sizeof(length));
  length = ntohl(length);

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

void Debugee::process(int timeout) {
  auto temp_buffer_ptr = _temp_buffer.data();

  while (poll(timeout)) {
    ssize_t bytes_read =
        ::read(_connection, temp_buffer_ptr, sizeof(_temp_buffer));

    if (!bytes_read) {
      throw std::runtime_error("Debugee::process: connection is lost.");
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

    if ((int)_command >= COMMAND_SIZE_TABLE.size()) {
      throw std::runtime_error("Debugee:process: invalid command type: " +
                               std::to_string((int)_command));
    }

    // not enough data
    const auto command_size = COMMAND_SIZE_TABLE[(int)_command];
    if (_accum_buffer.size() < command_size) {
      return;
    }

    process_command();

    _accum_buffer.erase(_accum_buffer.begin(),
                        _accum_buffer.begin() + command_size);
    _command = DebugCommand::NONE;
  }
}

void Debugee::process_command() {
  switch (_command) {
  case DebugCommand::BREAK: {
    std::uint32_t value;
    std::memcpy(&value, _accum_buffer.data(), sizeof(value));
    value = ntohl(value);

    if (_breakpoints.count(value)) {
      _breakpoints.erase(value);
      send_format("Breakpoint at 0x%X removed.", value);
      break;
    }

    _breakpoints.emplace(value);
    send_format("Breakpoint at 0x%X set.", value);
    break;
  }

  case DebugCommand::NEXT: {
    flags |= NEXT;
    flags &= ~PAUSED;
    break;
  }

  case layer::DebugCommand::SKIP: {
    flags |= SKIP;
    flags &= ~PAUSED;
    break;
  }

  case DebugCommand::PAUSE_MODE: {
    std::uint8_t value;
    std::memcpy(&value, _accum_buffer.data(), sizeof(value));

    if (value) {
      flags |= PAUSED;
    } else {
      flags &= ~PAUSED;
    }
    break;
  }

  case DebugCommand::PRINT_REGISTER: {
    std::uint32_t value;
    std::memcpy(&value, _accum_buffer.data(), sizeof(value));
    value = ntohl(value);

    send_format("r%d=0x%X (%u)", value, _ps.r[value], _ps.r[value]);
    break;
  }

  case DebugCommand::PRINT_AT_ADDRESS: {
    std::uint32_t value;
    std::memcpy(&value, _accum_buffer.data(), sizeof(value));
    value = ntohl(value);

    const std::uint8_t *ptr =
        reinterpret_cast<const std::uint8_t *>(_ps.address_resolve(value));
    send_format("0x%X=0x%X (%u)", value, *ptr, *ptr);
    break;
  }

  case DebugCommand::DUMP:
  case DebugCommand::RESTORE:
    break;

  default:
    throw std::runtime_error("Debugee::process_command: invalid command type.");
  }
}

void Debugee::stall() {
  while (flags & PAUSED)
    process(30); // wait for continue
}
} // namespace layer
