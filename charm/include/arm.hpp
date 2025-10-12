/* ARM Documentation:
 * https://iitd-plos.github.io/col718/ref/arm-instructionset.pdf
 */

#pragma once
#include <cstdint>
#include <string>
#include <variant>

namespace charm::arm {

typedef uint32_t addr_t;
typedef uint32_t instr_t;
struct Instruction;

enum class Register : std::uint8_t {
	R0,
	R1,
	R2,
	R3,
	R4,
	R5,
	R6,
	R7,
	R8,
	R9,
	R10,
	R11,

	IP, // r12 / intro-prodecure call
	SP, // stack pointer
	LR, // link register
	PC, // program counter (instr_addr + 8)

	COUNT,
};

enum class Opcode : std::uint8_t {
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

enum class Condition : std::uint8_t {
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
	enum {
		LSL, // logical shift left
		LSR, // logical shift right
		ASR, // arithmetic shift right
		ROR, // rotate right
	} type;

	bool is_reg;
	Register rm;               // Rm register to shift.
	std::uint8_t amount_or_rs; // Shift amount can be stored as an immediate
	                           // value or in a Rs register.

	static Shifter decode(instr_t value);
};

// 4.5 Data Processing
struct DataProcessing {
	Opcode op;
	Register rn, rd;

	union {
		Shifter op2_reg;
		uint32_t op2_imm;
	}; // operand 2

	static DataProcessing decode(Instruction &instr, instr_t value);
};

// 4.7 Multiply and Multiply-Accumulate (MUL, MLA)
struct Multiply {
	bool a; // accumulate
	Register rd, rn, rs, rm;

	static Multiply decode(Instruction &instr, instr_t value);
};

// 4.8 Multiply Long and Multiply-Accumulate Long (MULL,MLAL)
struct MultiplyLong {
	bool sign;             // unsigned (0) or signed (1)
	bool a;                // accumulate or not
	Register rd_hi, rd_lo; // low / high register to form a 32 bit value
	Register rs, rm;

	static MultiplyLong decode(Instruction &instr, instr_t value);
};

// 4.9 Single Data Transfer (LDR, STR)
struct DataTransfer {
	bool p;    // add offset after (0) or before (1) transfer?
	bool u;    // subtract (0) or add (1) offset from base?
	bool b;    // word (0) or byte (1)
	bool w;    // write address into base?
	bool load; // store (0) or Load (1)?

	Register rn, rd; // rn - base, rd - src/dst

	union {
		Shifter reg;  // shifted register as offset
		uint16_t imm; // immediate as offset
	};

	static DataTransfer decode(Instruction &instr, instr_t value);
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
		uint8_t imm; // offset as imm
	};

	static HalfWordDataTransfer decode(Instruction &instr, instr_t value,
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
	uint16_t reg_list; // register list, each bit is a register r0-r15

	static BlockDataTransfer decode(instr_t value);
};

// 4.12 Single Data Swap (SWP)
struct DataSwap {
	bool b; // word (0) or byte (1)
	Register rn, rd, rm;

	static DataSwap decode(instr_t value);
};

// 4.4 Branch and Branch with Link (B, BL)
struct Branch {
	bool link;           // write address to link register?
	std::int32_t offset; // NOTE: signed offset

	static Branch decode(instr_t value);
};

// 4.3 Branch and Exchange (BX)
struct BranchEx {
	Register rm;

	static BranchEx decode(instr_t value);
};

// 4.13 Software Interrupt (SWI)
struct SWI {
	static SWI decode(instr_t value);
};

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
};

struct Instruction {
	instr_t value = 0xFFFFFFFF; // raw representation of the instruction
	Condition condition = Condition::AL;
	bool immediate;  // is operand imm or reg?
	bool set_cflags; // will set condition flags?

	std::variant<DataProcessing, Multiply, MultiplyLong, DataTransfer,
	             HalfWordDataTransfer, BlockDataTransfer, DataSwap, Branch,
	             BranchEx, SWI>
	    group;

	static Instruction decode(instr_t value);

	std::string dump() const;
};

} // namespace charm::arm
