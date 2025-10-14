#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

namespace charm::runtime {

class CPUState;

// NOTE: see .cpp file for sizes
enum class DebugCommand : std::uint8_t {
	NONE,
	BREAK,            // set/remove breakpoint
	NEXT,             // skip to next step (either instruction or internal)
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
	enum {
		NONE = 0,
		PAUSED = 1 << 0,
		NEXT = 1 << 1,
		SKIP = 1 << 2,
	};
	std::uint32_t flags = PAUSED;

	Debugee(CPUState &ps);
	~Debugee();

	// these two functions are called either each instruction or inside
	// instruction impl
	// if STEP or SKIP set respectively, they shall set PAUSE flag.
	void next();
	void skip();

	template <typename... Args>
	inline void send_format(const std::string &fmt, Args... args) {
		format(fmt, args...);
		send_message();
	}

	template <typename... Args>
	inline void format(const std::string &fmt, Args... args) {
		std::memset(_temp_buffer.data(), 0, _temp_buffer.size());
		std::snprintf(reinterpret_cast<char *>(_temp_buffer.data()),
		              _temp_buffer.size(), fmt.c_str(), args...);
	}

	void send_message();
	void send_paused();

  private:
	CPUState &_ps;
	int _socket = -1, _connection = -1;
	DebugCommand _command =
	    DebugCommand::NONE; /* current command (to index into size array) */

	std::unordered_set<std::uint32_t> _breakpoints;

	std::array<char, 512> _temp_buffer = {
	    0}; /* temp buffer used for various opeartions, such as read,
	    snpritnf, etc... */
	std::vector<char> _accum_buffer; /* fill with data, then read packet */

	void stall();
	bool poll(int timeout);
	void process(int timeout);
	void process_command();
};

} // namespace charm::runtime
