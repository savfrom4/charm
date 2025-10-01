#include "liblayer/debug.hpp"
#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <stdexcept>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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

  std::cout << "Listening for connection on port " << LAYER_DEBUG_PORT
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

  std::cout << "Connection established." << std::endl;

  while (_flags & PAUSED) {
    poll(1000);
  }
}

Debugee::~Debugee() {
  if (_socket) {
    close(_socket);
  }

  if (_connection) {
    close(_connection);
  }
}

void Debugee::poll(int timeout) {
  while (connection_poll(timeout)) {
    ssize_t bytes_read = read(_connection, _recv_buffer, sizeof(_recv_buffer));
    if (!bytes_read) {
      throw std::runtime_error("poll: Connection lost.");
    }

    _accum_buffer.insert(_accum_buffer.end(), _recv_buffer,
                         _recv_buffer + bytes_read);
  }

  if (!_accum_buffer.size()) {
    return;
  }

  DebugeeCommand cmd;
}

bool Debugee::connection_poll(int timeout) {
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
        "connection_poll: invalid file descriptor (Connection lost?).");
  }

  return (fd.revents & POLLIN);
}

} // namespace layer
