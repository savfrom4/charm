/* ARM Documentation:
 * https://iitd-plos.github.io/col718/ref/arm-instructionset.pdf
 */

#pragma once
#include <arch.hpp>
#include <cstdint>
#include <string>
#include <variant>

namespace charm::isa::arm {

struct Instruction;

enum class Opcode {
	AND, // logical and
	EOR, // logical exclusive or
	SUB, // subtract (no carry)
	RSB, // reverse subtract (no carry)
	ADD, // add (no carry)
	ADC, // add (carry)
	SBC, // subtract (carry)
	RSC, // reverse subtract (carry)
	TST, // test bits
	TEQ, // test eql
	CMP, // compare
	CMN, // compare negative
	ORR, // logical or
	MOV, // move
	BIC, // bit clear
	MVN, // move not

	COUNT,
};

enum class Condition {
	EQ, // equal
	NE, // not equal
	CS, // carry set
	CC, // carry clear
	MI, // negative
	PL, // positive or zero
	VS, // overflow set
	VC, // overflow clear
	HI, // unsigned higher
	LS, // unsigned lower or same
	GE, // signed greater or equal
	LT, // signed less than
	GT, // signed greater than
	LE, // signed less than
	AL, // always
	NV, // never

	COUNT,
};

// 4.5.2 Shifts
struct Shifter {
	enum : Byte {
		LSL, // logical shift left
		LSR, // logical shift right
		ASR, // arithmetic shift right
		ROR, // rotate right
	} type;

	bool is_reg;
	Register rm;       // Rm register to shift.
	Byte amount_or_rs; // Shift amount can be stored as an immediate
	                   // value or in a Rs register.

	static constexpr Shifter decode(Word value);
};

// 4.5 Data Processing
struct DataProcessing {
	Opcode op;
	Register rn, rd;

	union {
		Shifter op2_reg;
		Word op2_imm;
	}; // operand 2

	static constexpr DataProcessing decode(Instruction &instr, Word value);
};

// 4.7 Multiply and Multiply-Accumulate (MUL, MLA)
struct Multiply {
	bool a; // accumulate
	Register rd, rn, rs, rm;

	static constexpr Multiply decode(Instruction &instr, Word value);
};

// 4.8 Multiply Long and Multiply-Accumulate Long (MULL,MLAL)
struct MultiplyLong {
	bool sign;             // unsigned (0) or signed (1)
	bool a;                // accumulate or not
	Register rd_hi, rd_lo; // low / high register to form a 32 bit value
	Register rs, rm;

	static constexpr MultiplyLong decode(Instruction &instr, Word value);
};

// 4.9 Single Data Transfer (LDR, STR)
struct DataTransfer {
	bool p;  // add offset after (0) or before (1) transfer?
	bool u;  // subtract (0) or add (1) offset from base?
	bool b;  // word (0) or byte (1)
	bool w;  // write address into base?
	bool ld; // store (0) or Load (1)?

	Register rn, rd; // rn - base, rd - src/dst

	union {
		Shifter reg;  // shifted register as offset
		Halfword imm; // immediate as offset
	};

	static constexpr DataTransfer decode(Instruction &instr, Word value);
};

// 4.10 Halfword and Signed Data Transfer
struct HalfWordDataTransfer {
	bool p;  // add offset after (0) or before (1) transfer?
	bool u;  // subtract (0) or add (1) offset from base?
	bool w;  // write address into base?
	bool ld; // store (0) or Load (1)?

	Register rn, rd; // rn - base, rd - src/dst

	enum {
		HALF_WORD = 0b01,        // unsigned half-word
		SIGNED_BYTE = 0b10,      // signed byte
		SIGNED_HALF_WORD = 0b11, // signed half-word
	} type;

	union {
		Register rm; // offset in register
		Byte imm;    // offset as imm
	};

	static constexpr HalfWordDataTransfer decode(Instruction &instr, Word value,
	                                             bool imm);
};

// 4.11 Block Data Transfer (LDM, STM)
struct BlockDataTransfer {
	bool p;   // add offset after (0) or before (1) transfer?
	bool u;   // subtract (0) or add (1) offset from base?
	bool psr; // unused for now
	bool w;   // write back address into base?
	bool ld;  // store (0) or Load (1)?

	Register rn;       // base
	Halfword reg_list; // register list, each bit is a register r0-r15

	static constexpr BlockDataTransfer decode(Word value);
};

// 4.12 Single Data Swap (SWP)
struct DataSwap {
	bool b; // word (0) or byte (1)
	Register rn, rd, rm;

	static constexpr DataSwap decode(Word value);
};

// 4.4 Branch and Branch with Link (B, BL)
struct Branch {
	bool link;           // write address to link register?
	std::int32_t offset; // NOTE: signed word

	static constexpr Branch decode(Word value);
};

// 4.3 Branch and Exchange (BX)
struct BranchEx {
	Register rm;

	static constexpr BranchEx decode(Word value);
};

// 4.13 Software Interrupt (SWI)
struct SWI {
	static constexpr SWI decode(Word value);
};

struct Invalid {};

enum class InstructionGroup {
	DATA_PROCESSING,
	MULTIPLY,
	MULTIPLY_LONG,
	DATA_TRANSFER,
	HALFWORD_DATA_TRANSFER,
	BLOCK_DATA_TRANSFER,
	DATA_SWAP,
	BRANCH,
	BRANCH_EXCHANGE,
	SWI,
	INVALID,
};

struct Instruction {
	constexpr Instruction(Word value);

	Condition condition = Condition::NV;
	Word value = 0xFFFFFFFF; // raw representation of the instruction
	bool is_imm = false;     // is operand imm or reg?
	bool set_cflags = false; // will set condition flags?

	std::variant<DataProcessing, Multiply, MultiplyLong, DataTransfer,
	             HalfWordDataTransfer, BlockDataTransfer, DataSwap, Branch,
	             BranchEx, SWI, Invalid>
	    group = Invalid{};

	std::string dump() const;
};

} // namespace charm::isa::arm

#include "arm.inl"
