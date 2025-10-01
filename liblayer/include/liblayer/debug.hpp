#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

// -------------------------------------
// ------------- OPTIONS ---------------
// -------------------------------------

#ifndef LAYER_DEBUG_PORT
#define LAYER_DEBUG_PORT (6969)
#endif

// following macros are used internally inside ExecutionState
// defines because debug context isn't always available (when LAYER_DEBUG is not
// set, for example)
#ifdef LAYER_DEBUG
#define LAYER_DBE_STEP_INSTR() _dbe.step_instruction()
#define LAYER_DBE_STEP_INTERN() _dbe.step_internal()
#define LAYER_DBE_REQUEST_PAUSE() _dbe.send_pause_request()
#define LAYER_DBE_LOG(fmt, ...) _dbe.send(fmt, __VA_ARGS__)
#else
#define LAYER_DBE_STEP_INSTR()
#define LAYER_DBE_STEP_INTERN()
#define LAYER_DBE_REQUEST_PAUSE()
#define LAYER_DBE_LOG(fmt, ...)
#endif

namespace layer {

class ExecutionState;

// NOTE: see .cpp file for sizes
enum class DebugCommand : std::uint8_t {
  NONE,
  BREAK,      // set/remove breakpoint
  STEP,       // step to next execution point (either instruction or internal)
  PAUSE_MODE, // set pause mode (pause/continue)
  PRINT_REGISTER,   // dump register
  PRINT_AT_ADDRESS, // dump unsigned byte at addr n
  DUMP,             // dump execution state
  RESTORE,          // restore execution state
  COUNT,
};

// debugee is a tcp listener that's used by charm-dbg
class Debugee {
public:
  Debugee();
  ~Debugee();

  // this function is called on each instruction, allows to process breakpoints
  // and pause execution if required
  void process_instruction(ExecutionState &ps);

  // this function is called internally inside the instruction impl
  // if INTERNAL flag is set, it allows to step inside the instruction internals
  void process_internal(ExecutionState &ps);

  void send_text(const std::string &fmt, ...);
  void send_pause_request();

private:
  int _socket = -1, _connection = -1;
  DebugCommand _command =
      DebugCommand::NONE; /* current command (to index into size array) */

  std::unordered_set<std::uint32_t> _breakpoints;

  enum {
    NONE = 0,
    PAUSED = 1 << 0,   // when set, public process_* call will stall and wait
                       // for continue cmd from debugger
    INTERNAL = 1 << 1, // see process_internal()
  } _flags = PAUSED;

  std::array<std::uint8_t, 512> _recv_buffer; /* read buffer */
  std::vector<std::uint8_t>
      _accum_buffer; /* fill with data, then read packet */

  bool poll(int timeout);
  void process(int timeout);
  void process_command();
};

} // namespace layer
