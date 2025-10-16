#pragma once
#include <arch.hpp>
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

}
