#include "arch.hpp"
#include "runtime/memory.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <endian.h>
#include <stdexcept>

#include <isa/arm.hpp>
#include <runtime/cpu.hpp>
#include <runtime/helpers.hpp>

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#warning "Untested on big-endian systems, expect problems!"
#endif

namespace charm::runtime {

template <bool is_imm, Word value, CPUState::CRefShifter shifter>
Word CPUState::value_or_shift() {
	return is_imm ? value : shift<shifter>();
}

template <CPUState::CRefShifter shifter> Word CPUState::shift() {
	Word value = r[shifter.rm];
	Word amount =
	    shifter.is_reg ? r[shifter.amount_or_rs] : shifter.amount_or_rs;

	// LSL
	if constexpr (shifter.type == isa::arm::Shifter::LSL) {
		if (!amount) {
			return value;
		}

		if (amount > 32) {
			C = false;
			return 0;
		}

		if (amount == 32) {
			C = (value & 1) != 0; // bit 0
			return 0;
		}

		C = (value & (1u << (32 - amount))) != 0; // last shifted bit
		return value << amount;
	}

	// LSR
	else if constexpr (shifter.type == isa::arm::Shifter::LSR) {
		if (!amount) {
			return value;
		}

		if (amount > 32) {
			C = false;
			return 0;
		}

		if (amount == 32) {
			C = (value & (1 << 31)) != 0; // bit 31
			return 0;
		}

		C = (value & (1u << (amount - 1))) != 0; // last shifted bit
		return value >> amount;
	}

	// ASR
	else if constexpr (shifter.type == isa::arm::Shifter::ASR) {
		if (!amount) {
			return value;
		}

		if (amount >= 32) {
			C = (value & 0x80000000) != 0;
			return C ? 0xFFFFFFFF : 0;
		}

		C = (value & (1u << (amount - 1))) != 0; // last shifted bit
		return ((int32_t)value) >> amount;
	}

	// ROR
	else {
		if (!amount || !(amount &= 0x1F)) {
			return value;
		}

		C = (value & (1u << (amount - 1))) != 0; // last shifted bit
		return (value >> amount) | (value << (32 - amount));
	}
}

template <CPUState::CRefInstr instr> void CPUState::arm_add() {
	constexpr const auto &data =
	    std::get<isa::arm::DataProcessing>(instr.group);
	const Word value =
	    value_or_shift<instr.is_imm, data.op2_imm, data.op2_reg>();

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	if (instr.set_cflags) {
		int32_t unused;
		C = __builtin_add_overflow(r[data.rn], value, &r[data.rd]);
		V = __builtin_sadd_overflow(r[data.rn], value, &unused);
		N = (r[data.rd] >> 31) & 1;
		Z = !r[data.rd];
	} else {
		r[data.rd] = r[data.rn] + value;
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr> void CPUState::arm_adc() {
	constexpr const auto &data =
	    std::get<isa::arm::DataProcessing>(instr.group);
	const Word value =
	    value_or_shift<instr.is_imm, data.op2_imm, data.op2_reg>();

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	Word operand = value + C;
	if (instr.set_cflags) {
		int32_t unused;
		C = __builtin_add_overflow(r[data.rn], operand, &r[data.rd]);
		V = __builtin_sadd_overflow(r[data.rn], operand, &unused);
		N = (r[data.rd] >> 31) & 1;
		Z = !r[data.rd];
	} else {
		r[data.rd] = r[data.rn] + operand;
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr> void CPUState::arm_sub() {
	constexpr const auto &data =
	    std::get<isa::arm::DataProcessing>(instr.group);
	const Word value =
	    value_or_shift<instr.is_imm, data.op2_imm, data.op2_reg>();

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	if (instr.set_cflags) {
		int32_t unused;
		C = !__builtin_sub_overflow(r[data.rn], value, &r[data.rd]);
		V = __builtin_ssub_overflow(r[data.rn], value, &unused);
		N = (r[data.rd] >> 31) & 1;
		Z = !r[data.rd];
	} else {
		r[data.rd] = r[data.rn] - value;
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr> void CPUState::arm_sbc() {
	constexpr const auto &data =
	    std::get<isa::arm::DataProcessing>(instr.group);
	const Word value =
	    value_or_shift<instr.is_imm, data.op2_imm, data.op2_reg>();

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	Word operand = value + !C;
	if (instr.set_cflags) {
		int32_t unused;
		C = !__builtin_sub_overflow(r[data.rn], operand, &r[data.rd]);
		V = __builtin_ssub_overflow(r[data.rn], operand, &unused);
		N = (r[data.rd] >> 31) & 1;
		Z = !r[data.rd];
	} else {
		r[data.rd] = r[data.rn] - operand;
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr> void CPUState::arm_cmp() {
	constexpr const auto &data =
	    std::get<isa::arm::DataProcessing>(instr.group);
	const Word value =
	    value_or_shift<instr.is_imm, data.op2_imm, data.op2_reg>();

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	uint32_t result;
	int32_t unused;

	C = !__builtin_sub_overflow(r[data.rn], value, &result);
	V = __builtin_ssub_overflow(r[data.rn], value, &unused);
	N = (result >> 31) & 1;
	Z = !result;

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr> void CPUState::arm_mov() {
	constexpr const auto &data =
	    std::get<isa::arm::DataProcessing>(instr.group);
	const Word value =
	    value_or_shift<instr.is_imm, data.op2_imm, data.op2_reg>();

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	r[data.rd] = value;

	if (instr.set_cflags) {
		N = (r[data.rd] >> 31) & 1;
		Z = !r[data.rd];
		UNAFFECTED(C);
		UNAFFECTED(V);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr> void CPUState::arm_rsb() {
	constexpr const auto &data =
	    std::get<isa::arm::DataProcessing>(instr.group);
	const Word value =
	    value_or_shift<instr.is_imm, data.op2_imm, data.op2_reg>();

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	if (instr.set_cflags) {
		int32_t unused;
		C = !__builtin_sub_overflow(value, r[data.rn], &r[data.rd]);
		V = __builtin_ssub_overflow(value, r[data.rn], &unused);
		N = (r[data.rd] >> 31) & 1;
		Z = !r[data.rd];
	} else {
		r[data.rd] = value - r[data.rn];
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr> void CPUState::arm_rsc() {
	constexpr const auto &data =
	    std::get<isa::arm::DataProcessing>(instr.group);
	const Word value =
	    value_or_shift<instr.is_imm, data.op2_imm, data.op2_reg>();

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	Word operand = r[data.rn] + !C;

	if (instr.set_cflags) {
		int32_t unused;
		C = !__builtin_sub_overflow(value, operand, &r[data.rd]);
		V = __builtin_ssub_overflow(value, operand, &unused);
		N = (r[data.rd] >> 31) & 1;
		Z = !r[data.rd];
	} else {
		r[data.rd] = value - operand;
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr> void CPUState::arm_and() {
	constexpr const auto &data =
	    std::get<isa::arm::DataProcessing>(instr.group);
	const Word value =
	    value_or_shift<instr.is_imm, data.op2_imm, data.op2_reg>();

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	r[data.rd] = r[data.rn] & value;

	if (instr.set_cflags) {
		N = (r[data.rd] >> 31) & 1;
		Z = !r[data.rd];
		UNAFFECTED(C);
		UNAFFECTED(V);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr> void CPUState::arm_eor() {
	constexpr const auto &data =
	    std::get<isa::arm::DataProcessing>(instr.group);
	const Word value =
	    value_or_shift<instr.is_imm, data.op2_imm, data.op2_reg>();

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	r[data.rd] = r[data.rn] ^ value;

	if (instr.set_cflags) {
		N = (r[data.rd] >> 31) & 1;
		Z = !r[data.rd];
		UNAFFECTED(C);
		UNAFFECTED(V);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr> void CPUState::arm_orr() {
	constexpr const auto &data =
	    std::get<isa::arm::DataProcessing>(instr.group);
	const Word value =
	    value_or_shift<instr.is_imm, data.op2_imm, data.op2_reg>();

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	r[data.rd] = r[data.rn] | value;
	if (instr.set_cflags) {
		N = (r[data.rd] >> 31) & 1;
		Z = !r[data.rd];
		UNAFFECTED(C);
		UNAFFECTED(V);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr> void CPUState::arm_bic() {
	constexpr const auto &data =
	    std::get<isa::arm::DataProcessing>(instr.group);
	const Word value =
	    value_or_shift<instr.is_imm, data.op2_imm, data.op2_reg>();

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	r[data.rd] = r[data.rn] & ~value;
	if (instr.set_cflags) {
		N = (r[data.rd] >> 31) & 1;
		Z = !r[data.rd];
		UNAFFECTED(C);
		UNAFFECTED(V);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr> void CPUState::arm_mvn() {
	constexpr const auto &data =
	    std::get<isa::arm::DataProcessing>(instr.group);
	const Word value =
	    value_or_shift<instr.is_imm, data.op2_imm, data.op2_reg>();

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	r[data.rd] = ~value;

	if (instr.set_cflags) {
		N = (r[data.rd] >> 31) & 1;
		Z = !r[data.rd];
		UNAFFECTED(C);
		UNAFFECTED(V);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr> void CPUState::arm_tst() {
	constexpr const auto &data =
	    std::get<isa::arm::DataProcessing>(instr.group);
	const Word value =
	    value_or_shift<instr.is_imm, data.op2_imm, data.op2_reg>();

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	Word result = r[data.rn] & value;

	// NOTE: s is ignored, flags are always set
	N = (result >> 31) & 1;
	Z = !result;
	UNAFFECTED(C);
	UNAFFECTED(V);

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr> void CPUState::arm_teq() {
	constexpr const auto &data =
	    std::get<isa::arm::DataProcessing>(instr.group);
	const Word value =
	    value_or_shift<instr.is_imm, data.op2_imm, data.op2_reg>();

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	Word result = r[data.rn] ^ value;

	// NOTE: s is ignored, flags are always set
	N = (result >> 31) & 1;
	Z = !result;
	UNAFFECTED(C);
	UNAFFECTED(V);

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr> void CPUState::arm_cmn() {
	constexpr const auto &data =
	    std::get<isa::arm::DataProcessing>(instr.group);
	const Word value =
	    value_or_shift<instr.is_imm, data.op2_imm, data.op2_reg>();

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	Word result;
	int32_t unused;

	// NOTE: s is ignored, flags are always set
	C = __builtin_add_overflow(r[data.rn], value, &result);
	V = __builtin_sadd_overflow(r[data.rn], value, &unused);
	N = (result >> 31) & 1;
	Z = !result;

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr> void CPUState::arm_mul() {
	constexpr const auto &mul = std::get<isa::arm::Multiply>(instr.group);

	UNPREDICTABLE(mul.rd == PC || mul.rm == PC || mul.rs == PC,
	              "arm_mul: Rd/Rm or Rs must not be PC.");
	UNPREDICTABLE(mul.rd == mul.rm,
	              "arm_mul: Rd and Rm must be different registers.");

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	r[mul.rd] = r[mul.rm] * r[mul.rs];
	if (instr.set_cflags) {
		N = (r[mul.rd] >> 31) & 1;
		Z = !r[mul.rd];
		UNAFFECTED(C);
		UNAFFECTED(V);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr> void CPUState::arm_mla() {
	constexpr const auto &mul = std::get<isa::arm::Multiply>(instr.group);

	UNPREDICTABLE(mul.rd == PC || mul.rm == PC || mul.rs == PC,
	              "arm_mla: Rd/Rm or Rs must not be PC.");
	UNPREDICTABLE(mul.rd == mul.rm,
	              "arm_mla: Rd and Rm must be different registers.");

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	r[mul.rd] = r[mul.rm] * r[mul.rs] + r[mul.rn];
	if (instr.set_cflags) {
		N = (r[mul.rd] >> 31) & 1;
		Z = !r[mul.rd];
		UNAFFECTED(C);
		UNAFFECTED(V);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr> void CPUState::arm_mull() {
	constexpr const auto &mull = std::get<isa::arm::MultiplyLong>(instr.group);

	UNPREDICTABLE(mull.rd_lo == PC || mull.rd_hi == PC || mull.rm == PC ||
	                  mull.rs == PC,
	              "arm_mlal: RdLo/RdHi/Rm or Rs must not be PC.");
	UNPREDICTABLE(mull.rd_lo == mull.rd_hi,
	              "arm_mlal: RdLo and RdHi must be different registers.");

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	uint64_t result;
	if (mull.sign) {
		result = (int64_t)r[mull.rm] * (int64_t)r[mull.rs];
	} else {
		result = (uint64_t)r[mull.rm] * (uint64_t)r[mull.rs];
	}

	r[mull.rd_hi] = (uint32_t)(result >> 32);
	r[mull.rd_lo] = (uint32_t)result;

	if (instr.set_cflags) {
		N = (result >> 63) & 1;
		Z = !result;
		UNAFFECTED(cf);
		UNAFFECTED(vf);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr> void CPUState::arm_mlal() {
	constexpr const auto &mull = std::get<isa::arm::MultiplyLong>(instr.group);

	UNPREDICTABLE(mull.rd_lo == PC || mull.rd_hi == PC || mull.rm == PC ||
	                  mull.rs == PC,
	              "arm_mlal: RdLo/RdHi/Rm or Rs must not be PC.");
	UNPREDICTABLE(mull.rd_lo == mull.rd_hi,
	              "arm_mlal: RdLo and RdHi must be different registers.");

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	uint64_t result;
	uint64_t acc = ((uint64_t)(r[mull.rd_hi]) << 32) | r[mull.rd_lo];

	if (mull.sign) {
		result = (int64_t)r[mull.rm] * (int64_t)r[mull.rs] + (int64_t)acc;
	} else {
		result = (uint64_t)r[mull.rm] * (uint64_t)r[mull.rs] + acc;
	}

	r[mull.rd_lo] = (uint32_t)result;
	r[mull.rd_hi] = (uint32_t)(result >> 32);

	if (instr.set_cflags) {
		N = (result >> 63) & 1;
		Z = !result;
		UNAFFECTED(C);
		UNAFFECTED(V);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr> void CPUState::arm_ldr(Memory &memory) {
	constexpr const auto &data_trans =
	    std::get<isa::arm::DataTransfer>(instr.group);

	UNPREDICTABLE((data_trans.w || data_trans.p) && r[data_trans.rn] == PC,
	              "Writeback with PC as Rn.")

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	const Word base = r[data_trans.rn];
	const Word offset =
	    value_or_shift<instr.is_imm, data_trans.imm, data_trans.reg>();
	const Word address =
	    data_trans.p ? base + (data_trans.u ? offset : -offset) : base;

	auto access = memory.access();
	if (data_trans.b) {
		r[data_trans.rd] = 0;
		access.load(address, &r[data_trans.rd], sizeof(Byte));
	} else {
		access.load(address, &r[data_trans.rd], sizeof(Word));
	}

	// writeback / post-indexing
	if (data_trans.w || !data_trans.p) {
		r[data_trans.rn] = base + (data_trans.u ? offset : -offset);
	}
}

template <CPUState::CRefInstr instr> void CPUState::arm_str(Memory &memory) {
	constexpr const auto &data_trans =
	    std::get<isa::arm::DataTransfer>(instr.group);

	UNPREDICTABLE((data_trans.w || data_trans.p) && r[data_trans.rn] == PC,
	              "Writeback with PC as Rn.")

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	Word base = r[data_trans.rn];
	Word value = r[data_trans.rd];

	const Word offset =
	    value_or_shift<instr.is_imm, data_trans.imm, data_trans.reg>();
	const Word address =
	    data_trans.p ? base + (data_trans.u ? offset : -offset) : base;

	/* SPECIAL CASE: When RD is PC, store ADDR + 12 */
	if (data_trans.rd == PC) {
		value += 4;
	}

	auto access = memory.access();
	if (data_trans.b) {
		access.store(address, &value, sizeof(Byte));
	} else {
		access.store(address, &value, sizeof(Word));
	}

	if (data_trans.w || !data_trans.p) {
		r[data_trans.rn] = base + (data_trans.u ? offset : -offset);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr> void CPUState::arm_ldrh(Memory &memory) {
	constexpr const auto &hw_data_trans =
	    std::get<isa::arm::HalfWordDataTransfer>(instr.group);

	UNPREDICTABLE((hw_data_trans.w || hw_data_trans.p) &&
	                  r[hw_data_trans.rn] == PC,
	              "Writeback with PC as Rn.")

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	Word base = r[hw_data_trans.rn];
	Word value = r[hw_data_trans.rd];

	/* SPECIAL CASE: When RD is PC, it will actually store ADDR + 12. Since PC
	 * is always ADDR + 8, we just add 4 to it. */
	if (hw_data_trans.rd == PC) {
		value += 4;
	}

	const Word offset = instr.is_imm ? hw_data_trans.imm : hw_data_trans.rm;
	const Word address =
	    hw_data_trans.p ? base + (hw_data_trans.u ? offset : -offset) : base;

	auto access = memory.access();
	if constexpr (hw_data_trans.type ==
	              isa::arm::HalfWordDataTransfer::HALF_WORD) {
		r[hw_data_trans.rd] = 0;
		access.load(address, &r[hw_data_trans.rd], sizeof(Halfword));
	}

	else if constexpr (hw_data_trans.type ==
	                   isa::arm::HalfWordDataTransfer::SIGNED_BYTE) {
		std::int8_t shw = 0;
		access.load(address, &shw, sizeof(shw));
		r[hw_data_trans.rd] = (std::int32_t)shw;
	}

	else if constexpr (hw_data_trans.type ==
	                   isa::arm::HalfWordDataTransfer::SIGNED_HALF_WORD) {
		std::int16_t shw = 0;
		access.load(address, &shw, sizeof(shw));
		r[hw_data_trans.rd] = (std::int32_t)shw;
	}

	if (hw_data_trans.w || !hw_data_trans.p) {
		r[hw_data_trans.rn] = base + (hw_data_trans.u ? offset : -offset);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr> void CPUState::arm_strh(Memory &memory) {
	constexpr const auto &hw_data_trans =
	    std::get<isa::arm::HalfWordDataTransfer>(instr.group);

	UNPREDICTABLE((hw_data_trans.w || hw_data_trans.p) &&
	                  r[hw_data_trans.rn] == PC,
	              "Writeback with PC as Rn.")

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	Word base = r[hw_data_trans.rn];
	Word value = r[hw_data_trans.rd];

	/* SPECIAL CASE: When RD is PC, it will actually store ADDR + 12. Since PC
	 * is always ADDR + 8, we just add 4 to it. */
	if (hw_data_trans.rd == PC) {
		value += 4;
	}

	const Word offset = instr.is_imm ? hw_data_trans.imm : hw_data_trans.rm;
	const Word address =
	    hw_data_trans.p ? base + (hw_data_trans.u ? offset : -offset) : base;

	auto access = memory.access();

	if constexpr (hw_data_trans.type ==
	              isa::arm::HalfWordDataTransfer::HALF_WORD) {
		r[hw_data_trans.rd] = 0;
		access.store(address, &value, sizeof(Halfword));
	}

	else if constexpr (hw_data_trans.type ==
	                   isa::arm::HalfWordDataTransfer::SIGNED_BYTE) {
		access.store(address, &value, sizeof(Byte));
	}

	else if constexpr (hw_data_trans.type ==
	                   isa::arm::HalfWordDataTransfer::SIGNED_HALF_WORD) {
		access.store(address, &value, sizeof(Byte));
	}

	switch (type) {
	case 0b01: // STRH
		memcpy(mem, &value, sizeof(uint16_t));
		break;

	case 0b10: // STRSB
		memcpy(mem, &value, sizeof(int8_t));
		break;

	case 0b11: // STRSH
		memcpy(mem, &value, sizeof(int16_t));
		break;
	}

	LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT,
	                 "value stored to %p: 0x%X", mem, r[rd]);

	if (w || !p) {
		UNPREDICTABLE(r[rn] == PC, "Writeback with PC as Rn.")

		r[rn] = base + (u ? offset : -offset);
		LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT,
		                 "wrote back to r%d: 0x%X", rn, r[rn]);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr>
void CPUState::arm_ldm(bool p, bool u, bool w, Register rn, Word reg_list) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	Word base = r[rn];
	Word n = __builtin_popcount(reg_list);
	Word addr;

	if (u) {
		addr = p ? base + 4 : base;
	} else {
		addr = p ? base - (n * 4) : base - 4;
	}

	if (w) {
		r[rn] = u ? base + n * 4 : base - n * 4;
		LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT,
		                 "wrote back to r%d: 0x%X", rn, r[rn]);
	}

	LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "virtual address: 0x%X",
	                 addr);

	const char *mem = reinterpret_cast<const char *>(_address_resolve(addr));

	LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "resolved to: %p", mem);

	if (UNLIKELY(!mem)) {
		LAYER_DBE_LOG(*this, "%s", "error: resolved address is 0x00000000!");
		LAYER_DBE_SEND_PAUSED(*this);

		throw std::runtime_error("arm_ldm: resolved address is 0x00000000");
	}

	for (Word i = 0; i < REGISTER_COUNT; i++) {
		if (!((reg_list >> i) & 1)) {
			continue;
		}

		LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "%s: before write",
		                 __func__);

		memcpy(&r[i], mem, sizeof(uint32_t));

		LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT,
		                 "value read from %p: 0x%X", mem, r[i]);

		LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "%s: after write",
		                 __func__);
		mem += 4;
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

template <CPUState::CRefInstr instr>
void CPUState::arm_stm(bool p, bool u, bool w, Register rn, Word reg_list) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	Word base = r[rn];
	Word n = __builtin_popcount(reg_list);
	Word addr;

	if (u) {
		addr = p ? base + 4 : base;
	} else {
		addr = p ? base - (n * 4) : base - 4;
	}

	LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "virtual address: 0x%X",
	                 addr);

	char *mem = reinterpret_cast<char *>(_address_resolve(addr));
	bool written = false;

	LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "resolved to: %p", mem);

	if (UNLIKELY(!mem)) {
		LAYER_DBE_LOG(*this, "%s", "error: resolved address is 0x00000000!");
		LAYER_DBE_SEND_PAUSED(*this);

		throw std::runtime_error("arm_ldm: resolved address is 0x00000000");
	}

	for (Word i = 0; i < REGISTER_COUNT; i++) {
		if (!((reg_list >> i) & 1)) {
			continue;
		}

		LAYER_DBE_NEXT(*this, "%s: before write", __func__);
		LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT,
		                 "value wrote to %p: 0x%X", mem, r[i]);

		memcpy(mem, &r[i], sizeof(uint32_t));
		mem += 4;

		LAYER_DBE_NEXT(*this, "%s: after write", __func__);

		if (!w || written) {
			continue;
		}

		// We write-back now
		r[rn] = u ? base + n * 4 : base - n * 4;
		written = true;

		LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT,
		                 "wrote back to r%d: 0x%X", rn, r[rn]);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}
} // namespace charm::runtime
