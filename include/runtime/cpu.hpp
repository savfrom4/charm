#pragma once
#include <array>
#include <cstdint>

#include <arch.hpp>
#include <runtime/memory.hpp>

namespace charm::runtime {

class CPUState {
  public:
	std::array<Word, Register::COUNT> r = {0};

	bool C = false, /* carry */
	    V = false;  /* overflow */
	bool N = false, /* negative */
	    Z = false;  /* zero */

	inline CPUState(const CPUState &) = delete;
	inline CPUState &operator=(const CPUState &) = delete;

	// NOTE: op2_value is THE final value of operand2, either immediate or
	// shifted register (paired with op2_* calls)

	void arm_add(bool s, Register rd, Register rn, Word op2_value);
	void arm_adc(bool s, Register rd, Register rn, Word op2_value);
	void arm_sub(bool s, Register rd, Register rn, Word op2_value);
	void arm_sbc(bool s, Register rd, Register rn, Word op2_value);
	void arm_cmp(bool s, Register rd, Register rn, Word op2_value);
	void arm_mov(bool s, Register rd, Register rn, Word op2_value);
	void arm_rsb(bool s, Register rd, Register rn, Word op2_value);
	void arm_rsc(bool s, Register rd, Register rn, Word op2_value);
	void arm_and(bool s, Register rd, Register rn, Word op2_value);
	void arm_eor(bool s, Register rd, Register rn, Word op2_value);
	void arm_orr(bool s, Register rd, Register rn, Word op2_value);
	void arm_bic(bool s, Register rd, Register rn, Word op2_value);
	void arm_mvn(bool s, Register rd, Register rn, Word op2_value);
	void arm_tst(bool s, Register rd, Register rn, Word op2_value);
	void arm_teq(bool s, Register rd, Register rn, Word op2_value);
	void arm_cmn(bool s, Register rd, Register rn, Word op2_value);

	void arm_mul(bool s, Register rd, Register rn, Register rs, Register rm);
	void arm_mla(bool s, Register rd, Register rn, Register rs, Register rm);

	void arm_mull(bool s, bool sign, Register rd_hi, Register rd_lo,
	              Register rs, Register rm);
	void arm_mlal(bool s, bool sign, Register rd_hi, Register rd_lo,
	              Register rs, Register rm);

	void arm_ldr(Memory &memory, bool p, bool u, bool b, bool w, Register rn,
	             Register rd, Word offset);
	void arm_str(Memory &memory, bool p, bool u, bool b, bool w, Register rn,
	             Register rd, Word offset);
	void arm_ldrh(Memory &memory, bool p, bool u, bool w, Register rn,
	              Register rd, uint8_t type, Word offset);
	void arm_strh(Memory &memory, bool p, bool u, bool w, Register rn,
	              Register rd, uint8_t type, Word offset);

	void arm_ldm(Memory &memory, bool p, bool u, bool w, Register rn,
	             Halfword reg_list);
	void arm_stm(Memory &memory, bool p, bool u, bool w, Register rn,
	             Halfword reg_list);

  private:
};

} // namespace charm::runtime
