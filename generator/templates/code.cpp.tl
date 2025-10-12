#include "code.hpp"
#include "data.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace layer;

#define EXPORT(name, address)                                                  \
	__attribute__((weak)) void ProgramState::name() {                          \
		LAYER_DBE_SKIP(*this, "call: %s", #name);                              \
		r[LR] = INSTR_RETURN_LR;                                               \
		eval(address);                                                         \
	}

#define STUB(name)                                                             \
	__attribute__((weak)) void ProgramState::name() {                          \
		std::cout << "stub: " << #name << std::endl;                           \
		LAYER_DBE_LOG(*this, "stub: %s", #name);                               \
	}

#define INSTR(addr)                                                            \
	case addr: {                                                               \
		a##addr : r[PC] = addr + 8;                                            \
	}

#define JUMP(addr)                                                             \
	address = addr;                                                            \
	goto __start__;

/* ADDRESS MAPPING */

std::uint32_t ProgramState::address_map_raw(std::uintptr_t address) {
	std::uint32_t mapped;
	if ((mapped = ExecutionState::address_map_raw(address))) {
		return mapped;
	}

	// clang-format off
/*% address_map %*/
	// clang-format on

	LAYER_DBE_LOG(*this, "Error: unable to map address: 0x%X!", address);
	LAYER_DBE_SEND_PAUSED(*this);
	throw std::runtime_error("address_map: unable to map address!");
}

std::uintptr_t ProgramState::address_resolve_raw(std::uint32_t address) {
	std::uintptr_t mapped;
	if ((mapped = ExecutionState::address_resolve_raw(address))) {
		return mapped;
	}

	// clang-format off
/*% address_resolve %*/
	// clang-format on

	LAYER_DBE_LOG(*this, "Error: unable to resolve address: 0x%X!", address);
	LAYER_DBE_SEND_PAUSED(*this);
	throw std::runtime_error("address_map: unable to resolve address!");
}

// clang-format off

// exported functions
/*% exported_functions %*/

// external functions
/*% external_functions %*/


void ProgramState::eval(std::uint32_t address) {
__start__:
	switch (address) {
	default: {
		throw std::runtime_error(
		    "ProgramState::eval: Cannot jump to an invalid address: " +
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
