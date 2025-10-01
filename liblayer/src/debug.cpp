#include "liblayer/debug.hpp"
#include <arpa/inet.h>
#include <cstddef>
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
const size_t PACKET_SIZE_TABLE[(int)layer::DebugCommand::COUNT] = {
    0,
    sizeof(std::uint32_t), // BREAK (32-bit imm address)
    sizeof(std::uint8_t),  // STEP_MODE (8-bit boolean)
    sizeof(std::uint8_t),  // PAUSE_MODE (8-bit boolean)
    sizeof(std::uint8_t),  // PRINT_REGISTER (8-bit register index)
    sizeof(std::uint32_t), // PRINT_AT_ADDRESS (32-bit imm address)
    sizeof(std::uint32_t), // DUMP (32-bit imm filename length + n bytes of
                           // string)
    sizeof(std::uint32_t), // RESTORE (32-bit imm filename length + n bytes of
                           // string)
};

namespace layer {

Debugee::Debugee() {
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

  if (!bind(_socket, (struct sockaddr *)&address, sizeof(address))) {
    throw std::runtime_error("ExecutionDebugee ctor: failed to bind socket.");
  }

  std::cout << "Waitng for debugger on port " << LAYER_DEBUG_PORT << "..."
            << std::endl;

  if (!listen(_socket, 1)) {
    throw std::runtime_error(
        "ExecutionDebugee ctor: failed to listen on socket.");
  }

  if (!(_connection = accept(_socket, (struct sockaddr *)&address,
                             (socklen_t *)&address))) {
    throw std::runtime_error(
        "ExecutionDebugee ctor: failed to accept the connection.");
  }

  // disable nagle's algorithm
  int one = 1;
  if (!setsockopt(_connection, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one))) {
    throw std::runtime_error("ExecutionDebugee ctor: failed to set NODELAY.");
  }

  std::cout << "Connection established." << std::endl;

  while (_flags & PAUSED)
    process(1000); // wait for continue
}

Debugee::~Debugee() {
  if (_socket) {
    close(_socket);
  }

  if (_connection) {
    close(_connection);
  }
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
  auto recv_buffer_ptr = _recv_buffer.data();

  while (poll(timeout)) {
    ssize_t bytes_read =
        ::read(_connection, recv_buffer_ptr, sizeof(_recv_buffer));

    if (!bytes_read) {
      throw std::runtime_error("Debugee::poll: connection is lost.");
    }

    _accum_buffer.insert(_accum_buffer.end(), recv_buffer_ptr,
                         recv_buffer_ptr + bytes_read);
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
    const auto command_size = PACKET_SIZE_TABLE[(int)_command];
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

    if (_breakpoints.count(value)) {
      _breakpoints.erase(value);
      send_text("Breakpoint at 0x%X removed.", value);
      break;
    }

    _breakpoints.emplace(value);
    send_text("Breakpoint at 0x%X set.", value);
    break;
  }

  case DebugCommand::CONTINUE_STEP_MODE: {
    std::uint8_t value;
    std::memcpy(&value, _accum_buffer.data(), sizeof(value));

    break;
  }

  case DebugCommand::PAUSE_MODE:
  case DebugCommand::PRINT_REGISTER:
  case DebugCommand::PRINT_AT_ADDRESS:
  case DebugCommand::DUMP:
  case DebugCommand::RESTORE:

  default:
    throw std::runtime_error("Debugee::process_command: invalid command type.");
  }
}

} // namespace layer
