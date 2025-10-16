#include "code.hpp"
#include "data.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace layer;

#define EXPORT(name, address)                                                  \
	__attribute__((weak)) void ExecutionState::name() {                          \
		std::cout << "call: " << #name << std::endl;                                  \
		cpu.r[LR] = INSTR_RETURN_LR;                                               \
		eval(address);                                                         \
	}

#define STUB(name)                                                             \
	__attribute__((weak)) void ExecutionState::name() {                          \
		std::cout << "stub: " << #name << std::endl;                           \
	}

#define INSTR(addr)                                                            \
	case addr: {                                                               \
		a##addr : cpu.r[PC] = addr + 8;                                            \
	}

#define JUMP(addr)                                                             \
	address = addr;                                                            \
	goto __start__;

// clang-format off

// exported functions
/*% exported_functions %*/

// external functions
/*% external_functions %*/

void ExecutionState::eval(std::uint32_t address) {
__start__:
	switch (address) {
	default: {
		throw std::runtime_error(
		    "ExecutionState::eval: Cannot jump to an invalid address: " +
		    std::to_string(address));
	}

	// when eval is called LR is set to a magic address
	// to be able to return out of the function.
	INSTR(INSTR_RETURN_LR) {
	    return;
	}

	// mapping external functions to their .got addresses
/*% got_mappings %*/

	// sections
/*% sections %*/
	}
}

// clang-format on
