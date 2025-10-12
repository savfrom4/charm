#pragma once
#include "memory.hpp"
#include <array>
#include <cstdint>
#include <mutex>

namespace layer {

typedef std::uint32_t reg_value_t;

// this couldve been an enum class
// but we need it to be easily castable to an integer
// + compactness
enum Register : std::uint8_t {
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
	R12 = 12,

	SP = 13,
	R13 = 13,

	LR = 14,
	R14 = 14,

	PC = 15,
	R15 = 15,

	REGISTER_COUNT = 16,
};

class ExecutionState {
  public:
// connection to the debugger, not always present
#ifdef LAYER_DEBUG
	Debugee dbe{*this};
#endif

	bool C = false, /* carry */
	    V = false;  /* overflow */
	bool N = false, /* negative */
	    Z = false;  /* zero */

	std::array<reg_value_t, REGISTER_COUNT> r = {0};

	VirtualMemory memory;

	inline ExecutionState(const ExecutionState &) = delete;
	inline ExecutionState &operator=(const ExecutionState &) = delete;

	void free(void *p);

	// armv4.cpp

	// NOTE: op2_value is THE final value of operand2, either immediate or
	// shifted register (paired with op2_* calls)

	void arm_add(bool s, Register rd, Register rn, reg_value_t op2_value);
	void arm_adc(bool s, Register rd, Register rn, reg_value_t op2_value);
	void arm_sub(bool s, Register rd, Register rn, reg_value_t op2_value);
	void arm_sbc(bool s, Register rd, Register rn, reg_value_t op2_value);
	void arm_cmp(bool s, Register rd, Register rn, reg_value_t op2_value);
	void arm_mov(bool s, Register rd, Register rn, reg_value_t op2_value);
	void arm_rsb(bool s, Register rd, Register rn, reg_value_t op2_value);
	void arm_rsc(bool s, Register rd, Register rn, reg_value_t op2_value);
	void arm_and(bool s, Register rd, Register rn, reg_value_t op2_value);
	void arm_eor(bool s, Register rd, Register rn, reg_value_t op2_value);
	void arm_orr(bool s, Register rd, Register rn, reg_value_t op2_value);
	void arm_bic(bool s, Register rd, Register rn, reg_value_t op2_value);
	void arm_mvn(bool s, Register rd, Register rn, reg_value_t op2_value);
	void arm_tst(bool s, Register rd, Register rn, reg_value_t op2_value);
	void arm_teq(bool s, Register rd, Register rn, reg_value_t op2_value);
	void arm_cmn(bool s, Register rd, Register rn, reg_value_t op2_value);

	void arm_mul(bool s, Register rd, Register rn, Register rs, Register rm);
	void arm_mla(bool s, Register rd, Register rn, Register rs, Register rm);

	void arm_mull(bool s, bool sign, Register rd_hi, Register rd_lo,
	              Register rs, Register rm);
	void arm_mlal(bool s, bool sign, Register rd_hi, Register rd_lo,
	              Register rs, Register rm);

	void arm_ldr(bool p, bool u, bool b, bool w, Register rn, Register rd,
	             reg_value_t offset);
	void arm_str(bool p, bool u, bool b, bool w, Register rn, Register rd,
	             reg_value_t offset);
	void arm_ldm(bool p, bool u, bool w, Register rn, reg_value_t reg_list);
	void arm_stm(bool p, bool u, bool w, Register rn, reg_value_t reg_list);
	void arm_ldrh(bool p, bool u, bool w, Register rn, Register rd,
	              uint8_t type, reg_value_t offset);
	void arm_strh(bool p, bool u, bool w, Register rn, Register rd,
	              uint8_t type, reg_value_t offset);

	// TODO: implement armv5, add thumbv1
};

} // namespace layer
