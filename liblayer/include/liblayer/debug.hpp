#pragma once
#include <array>
#include <cstdint>
#include <cstring>
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
#define LAYER_DBE_STEP(ps) (ps).dbe.step(ps)
#define LAYER_DBE_SKIP(ps, info) (ps).dbe.skip(ps, info)
#define LAYER_DBE_LOG(ps, fmt, ...) (ps).dbe.send_fmt(fmt, __VA_ARGS__)
#define LAYER_DBE_SEND_PAUSED(ps) (ps).dbe.send_paused(ps)
#else
#define LAYER_DBE_STEP(ps)
#define LAYER_DBE_SKIP(ps, info)
#define LAYER_DBE_LOG(ps, fmt, ...)
#define LAYER_DBE_SEND_PAUSED(ps)
#endif

namespace layer {

class ExecutionState;

// NOTE: see .cpp file for sizes
enum class DebugCommand : std::uint8_t {
  NONE,
  BREAK,            // set/remove breakpoint
  STEP,             // skip to next step (either instruction or internal)
  SKIP,             // skip to next instruction
  PAUSE_MODE,       // set pause mode (pause/continue)
  PRINT_REGISTER,   // dump register
  PRINT_AT_ADDRESS, // dump unsigned byte at addr n
  DUMP,             // dump execution state
  RESTORE,          // restore execution state
  COUNT,
};

// debugee is a tcp listener that's used by charm-dbg
class Debugee {
public:
  Debugee(ExecutionState &ps);
  ~Debugee();

  // these two functions are called either each instruction or inside
  // instruction impl
  // if STEP or SKIP set respectively, they shall set PAUSE flag.
  void step(ExecutionState &ps);
  void skip(ExecutionState &ps, const std::string &info);

  template <typename... Args>
  inline void send_fmt(const std::string &fmt, Args... args) {
    std::memset(_temp_buffer.data(), 0, _temp_buffer.size());
    std::snprintf(reinterpret_cast<char *>(_temp_buffer.data()),
                  _temp_buffer.size(), fmt.c_str(), args...);
    send_raw();
  }

  void send_paused(ExecutionState &ps);

private:
  int _socket = -1, _connection = -1;
  DebugCommand _command =
      DebugCommand::NONE; /* current command (to index into size array) */

  std::unordered_set<std::uint32_t> _breakpoints;

  enum {
    NONE = 0,
    PAUSED = 1 << 0, // when set, public process_* call will stall and wait
                     // for continue cmd from debugger
    STEP = 1 << 1,   // when set, will execute until
    SKIP = 1 << 2,
  };
  std::uint32_t _flags = PAUSED;

  std::array<std::uint8_t, 1024>
      _temp_buffer; /* temp buffer used for various opeartions, such as read,
                      snpritnf, etc... */
  std::vector<std::uint8_t>
      _accum_buffer; /* fill with data, then read packet */

  void send_raw();

  bool poll(int timeout);
  void process(ExecutionState &ps, int timeout);
  void process_command(ExecutionState &ps);
};

} // namespace layer
