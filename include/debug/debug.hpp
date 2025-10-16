#pragma once
#include <arch.hpp>
#include <array>
#define CHARM_DEBUG_PORT (22869)

namespace charm::debug {

enum class Command : Byte {
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

// NOTE: excludes 1 byte header
inline const std::array<Word, (int)debug::Command::COUNT> COMMAND_SIZE_TABLE = {
    0,
    sizeof(Word), // BREAK (32-bit imm address)
    0,            // STEP
    0,            // SKIP
    sizeof(Byte), // PAUSE_MODE (8-bit boolean)
    sizeof(Word), // PRINT_REGISTER (8-bit register index)
    sizeof(Word), // PRINT_AT_ADDRESS (32-bit imm address)
    sizeof(Word), // DUMP (32-bit imm filename length + n bytes of
                  // string)
    sizeof(Word), // RESTORE (32-bit imm filename length + n bytes of
                  // string)
};

} // namespace charm::debug
