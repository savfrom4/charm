#pragma once
#include <cstdint>
#include <string>
#include <vector>

#ifdef LAYER_DEBUG
#define LAYER_DBE_STEPIN() _dbg.stepin()
#define LAYER_DBE_BREAK() _dbg.send_break()
#define LAYER_DBE_LOG(fmt, ...) _dbg.send(fmt, __VA_ARGS__)
#else
#define LAYER_DBE_STEPIN()
#define LAYER_DBE_BREAK()
#define LAYER_DBE_LOG(fmt, ...)
#endif

#ifndef LAYER_DEBUG_PORT
#define LAYER_DEBUG_PORT (6969)
#endif

namespace layer {

class ExecutionState;

enum class DebugeeCommand : std::uint8_t {
  BREAK,

};

// connection to the debugger.
class Debugee {
public:
  Debugee();
  ~Debugee();

  void stepin(ExecutionState &ps);
  void stepover(ExecutionState &ps);
  void send(const std::string &fmt, ...);
  void send_break();

private:
  int _socket = 0, _connection = 0;

  std::vector<std::uint8_t> _accum_buffer;
  char _recv_buffer[1024];

  enum {
    NONE = 0,
    PAUSED = 1 << 0,
  } _flags = PAUSED;

  void poll(int timeout = 0);
  bool connection_poll(int timeout);
};

} // namespace layer
