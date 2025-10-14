#pragma once
#include <cstdint>

namespace charm {

using Dword = std::uint64_t;
using Word = std::uint32_t;
using Halfword = std::uint16_t;
using Byte = std::uint8_t;

enum Register : Byte {
	R0 = 0,
	R1 = 1,
	R2 = 2,
	R3 = 3,
	R4 = 4,
	R5 = 5,
	R6 = 6,
	R7 = 7,
	R8 = 8,
	R9 = 9,
	R10 = 10,
	R11 = 11,

	IP = 12,
	R12 = 12,

	// stack pointer
	SP = 13,
	R13 = 13,

	// link register (return addr.)
	LR = 14,
	R14 = 14,

	// program counter (curren instr. + 8)
	PC = 15,
	R15 = 15,

	COUNT,
};

} // namespace charm
