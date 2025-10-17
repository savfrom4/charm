#pragma once
#include <array>

#include <arch.hpp>
#include <isa/arm.hpp>
#include <runtime/memory.hpp>

namespace charm::runtime {

class CPUState {
  public:
	using CRefInstr = const isa::arm::Instruction;
	using CRefShifter = const isa::arm::Shifter &;

	std::array<Word, Register::COUNT> r = {0};

	bool C = false, /* carry */
	    V = false;  /* overflow */
	bool N = false, /* negative */
	    Z = false;  /* zero */

	inline CPUState() {}
	inline CPUState(const CPUState &) = delete;
	inline CPUState &operator=(const CPUState &) = delete;

	// NOTE: op2_value is THE final value of operand2, either immediate or
	// shifted register (paired with op2_* calls)

	template <CRefInstr> void arm_add();
	template <CRefInstr> void arm_adc();
	template <CRefInstr> void arm_sub();
	template <CRefInstr> void arm_sbc();
	template <CRefInstr> void arm_cmp();
	template <CRefInstr> void arm_mov();
	template <CRefInstr> void arm_rsb();
	template <CRefInstr> void arm_rsc();
	template <CRefInstr> void arm_and();
	template <CRefInstr> void arm_eor();
	template <CRefInstr> void arm_orr();
	template <CRefInstr> void arm_bic();
	template <CRefInstr> void arm_mvn();
	template <CRefInstr> void arm_tst();
	template <CRefInstr> void arm_teq();
	template <CRefInstr> void arm_cmn();

	template <CRefInstr> void arm_mul();
	template <CRefInstr> void arm_mla();

	template <CRefInstr> void arm_mull();
	template <CRefInstr> void arm_mlal();

	template <CRefInstr> void arm_ldr(Memory &memory);
	template <CRefInstr> void arm_str(Memory &memory);
	template <CRefInstr> void arm_ldrh(Memory &memory);
	template <CRefInstr> void arm_strh(Memory &memory);

	template <CRefInstr> void arm_ldm(Memory &memory);
	template <CRefInstr> void arm_stm(Memory &memory);

  private:
	template <bool, CRefShifter> Word _shift();
};

} // namespace charm::runtime

#include "inl/armv4.inl"
