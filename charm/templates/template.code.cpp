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
		LAYER_DBE_LOG(*this, "stub: %s", #name);                               \
	}

#define INSTR(addr)                                                            \
	case addr: {                                                               \
		a##addr : r[PC] = addr + 8;                                            \
	}

#define JUMP(addr)                                                             \
	address = addr;                                                            \
	goto __start__;

#define EQ(x)                                                                  \
	if (z) {                                                                   \
		x;                                                                     \
	}

#define NE(x)                                                                  \
	if (!z) {                                                                  \
		x;                                                                     \
	}

#define CS(x)                                                                  \
	if (cs) {                                                                  \
		x;                                                                     \
	}

#define CC(x)                                                                  \
	if (!cs) {                                                                 \
		x;                                                                     \
	}

#define MI(x)                                                                  \
	if (mi) {                                                                  \
		x;                                                                     \
	}

#define PL(x)                                                                  \
	if (!mi) {                                                                 \
		x;                                                                     \
	}

#define VS(x)                                                                  \
	if (vs) {                                                                  \
		x;                                                                     \
	}

#define VC(x)                                                                  \
	if (!vs) {                                                                 \
		x;                                                                     \
	}

#define HI(x)                                                                  \
	if (cs && !z) {                                                            \
		x;                                                                     \
	}

#define LS(x)                                                                  \
	if (!cs || z) {                                                            \
		x;                                                                     \
	}

#define GE(x)                                                                  \
	if (mi == vs) {                                                            \
		x;                                                                     \
	}

#define LT(x)                                                                  \
	if (mi != vs) {                                                            \
		x;                                                                     \
	}

#define GT(x)                                                                  \
	if (!z && (mi == vs)) {                                                    \
		x;                                                                     \
	}

#define LE(x)                                                                  \
	if (z || (mi != vs)) {                                                     \
		x;                                                                     \
	}

#define AL(x)                                                                  \
	if (1) {                                                                   \
		x;                                                                     \
	}

#define NV(x)                                                                  \
	if (0) {                                                                   \
		x;                                                                     \
	}

constexpr inline reg_value_t op2_lsl(ExecutionState &ps, bool s,
                                     reg_value_t value, reg_value_t amount) {
	if (!amount)
		return value;

	if (amount > 32) {
		ps.cs = false;
		return 0;
	}

	if (amount == 32) {
		ps.cs = (value & 1) != 0; // bit 0
		return 0;
	}

	ps.cs = (value & (1u << (32 - amount))) != 0; // last shifted bit
	return value << amount;
}

constexpr inline reg_value_t op2_lsr(ExecutionState &ps, bool s,
                                     reg_value_t value, reg_value_t amount) {
	if (!amount)
		return value;

	if (amount > 32) {
		ps.cs = false;
		return 0;
	}

	if (amount == 32) {
		ps.cs = (value & (1 << 31)) != 0; // bit 31
		return 0;
	}

	ps.cs = (value & (1u << (amount - 1))) != 0; // last shifted bit
	return value >> amount;
}

constexpr inline reg_value_t op2_asr(ExecutionState &ps, bool s,
                                     reg_value_t value, reg_value_t amount) {
	if (!amount)
		return value;

	if (amount >= 32) {
		ps.cs = (value & 0x80000000) != 0;
		return ps.cs ? 0xFFFFFFFF : 0;
	}

	ps.cs = (value & (1u << (amount - 1))) != 0; // last shifted bit
	return ((int32_t)value) >> amount;
}

constexpr inline reg_value_t op2_ror(ExecutionState &ps, bool s,
                                     reg_value_t value, reg_value_t amount) {
	if (!amount)
		return value;

	if (!(amount &= 0x1F)) {
		return value;
	}

	ps.cs = (value & (1u << (amount - 1))) != 0; // last shifted bit
	return (value >> amount) | (value << (32 - amount));
}

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
