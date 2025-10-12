#include "liblayer/debug.hpp"
#include "liblayer/execution_state.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <endian.h>
#include <stdexcept>

#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define UNPREDICTABLE(x, fmt, ...)                                             \
	if (UNLIKELY(x)) {                                                         \
		LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT,                     \
		                 "UNPREDICTABLE: " fmt, __VA_ARGS__);                  \
	}
#define UNAFFECTED(x)

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#warning "Untested on big-endian systems, expect problems!"
#endif

namespace layer {

void ExecutionState::arm_add(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	if (s) {
		int32_t unused;
		C = __builtin_add_overflow(r[rn], op2_value, &r[rd]);
		V = __builtin_sadd_overflow(r[rn], op2_value, &unused);
		N = (r[rd] >> 31) & 1;
		Z = !r[rd];
	} else {
		r[rd] = r[rn] + op2_value;
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_adc(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	reg_value_t operand = op2_value + C;
	if (s) {
		int32_t unused;
		C = __builtin_add_overflow(r[rn], operand, &r[rd]);
		V = __builtin_sadd_overflow(r[rn], operand, &unused);
		N = (r[rd] >> 31) & 1;
		Z = !r[rd];
	} else {
		r[rd] = r[rn] + operand;
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_sub(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	if (s) {
		int32_t unused;
		C = !__builtin_sub_overflow(r[rn], op2_value, &r[rd]);
		V = __builtin_ssub_overflow(r[rn], op2_value, &unused);
		N = (r[rd] >> 31) & 1;
		Z = !r[rd];
	} else {
		r[rd] = r[rn] - op2_value;
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_sbc(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	reg_value_t operand = op2_value + !C;
	if (s) {
		int32_t unused;
		C = !__builtin_sub_overflow(r[rn], operand, &r[rd]);
		V = __builtin_ssub_overflow(r[rn], operand, &unused);
		N = (r[rd] >> 31) & 1;
		Z = !r[rd];
	} else {
		r[rd] = r[rn] - operand;
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_cmp(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	uint32_t result;
	int32_t unused;

	C = !__builtin_sub_overflow(r[rn], op2_value, &result);
	V = __builtin_ssub_overflow(r[rn], op2_value, &unused);
	N = (result >> 31) & 1;
	Z = !result;

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_mov(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	r[rd] = op2_value;
	if (s) {
		N = (r[rd] >> 31) & 1;
		Z = !r[rd];
		UNAFFECTED(C);
		UNAFFECTED(V);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_rsb(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	if (s) {
		int32_t unused;
		C = !__builtin_sub_overflow(op2_value, r[rn], &r[rd]);
		V = __builtin_ssub_overflow(op2_value, r[rn], &unused);
		N = (r[rd] >> 31) & 1;
		Z = !r[rd];
	} else {
		r[rd] = op2_value - r[rn];
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_rsc(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	reg_value_t operand = r[rn] + !C;

	if (s) {
		int32_t unused;
		C = !__builtin_sub_overflow(op2_value, operand, &r[rd]);
		V = __builtin_ssub_overflow(op2_value, operand, &unused);
		N = (r[rd] >> 31) & 1;
		Z = !r[rd];
	} else {
		r[rd] = op2_value - operand;
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_and(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	r[rd] = r[rn] & op2_value;

	if (s) {
		N = (r[rd] >> 31) & 1;
		Z = !r[rd];
		UNAFFECTED(C);
		UNAFFECTED(V);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_eor(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	r[rd] = r[rn] ^ op2_value;

	if (s) {
		N = (r[rd] >> 31) & 1;
		Z = !r[rd];
		UNAFFECTED(C);
		UNAFFECTED(V);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_orr(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	r[rd] = r[rn] | op2_value;
	if (s) {
		N = (r[rd] >> 31) & 1;
		Z = !r[rd];
		UNAFFECTED(C);
		UNAFFECTED(V);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_bic(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	r[rd] = r[rn] & ~op2_value;
	if (s) {
		N = (r[rd] >> 31) & 1;
		Z = !r[rd];
		UNAFFECTED(C);
		UNAFFECTED(V);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_mvn(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	r[rd] = ~op2_value;

	if (s) {
		N = (r[rd] >> 31) & 1;
		Z = !r[rd];
		UNAFFECTED(C);
		UNAFFECTED(V);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_tst(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	reg_value_t result = r[rn] & op2_value;

	// NOTE: s is ignored, flags are always set
	N = (result >> 31) & 1;
	Z = !result;
	UNAFFECTED(C);
	UNAFFECTED(V);

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_teq(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	reg_value_t result = r[rn] ^ op2_value;

	// NOTE: s is ignored, flags are always set
	N = (result >> 31) & 1;
	Z = !result;
	UNAFFECTED(C);
	UNAFFECTED(V);

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_cmn(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	reg_value_t result;
	int32_t unused;

	// NOTE: s is ignored, flags are always set
	C = __builtin_add_overflow(r[rn], op2_value, &result);
	V = __builtin_sadd_overflow(r[rn], op2_value, &unused);
	N = (result >> 31) & 1;
	Z = !result;

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_mul(bool s, Register rd, Register rn, Register rs,
                             Register rm) {
	UNPREDICTABLE(rd == PC || rm == PC || rs == PC,
	              "arm_mul: Rd/Rm or Rs must not be PC.");
	UNPREDICTABLE(rd == rm, "arm_mul: Rd and Rm must be different registers.");

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	r[rd] = r[rm] * r[rs];

	if (s) {
		N = (r[rd] >> 31) & 1;
		Z = !r[rd];
		UNAFFECTED(C);
		UNAFFECTED(V);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_mla(bool s, Register rd, Register rn, Register rs,
                             Register rm) {
	UNPREDICTABLE(rd == PC || rm == PC || rs == PC,
	              "arm_mla: Rd/Rm or Rs must not be PC.");
	UNPREDICTABLE(rd == rm, "arm_mla: Rd and Rm must be different registers.");

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	r[rd] = r[rm] * r[rs] + r[rn];
	if (s) {
		N = (r[rd] >> 31) & 1;
		Z = !r[rd];
		UNAFFECTED(C);
		UNAFFECTED(V);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_mull(bool s, bool sign, Register rd_lo, Register rd_hi,
                              Register rm, Register rs) {
	UNPREDICTABLE(rd_lo == PC || rd_hi == PC || rm == PC || rs == PC,
	              "arm_mull: RdLo/RdHi/Rm or Rs must not be PC.");
	UNPREDICTABLE(rd_lo == rd_hi,
	              "arm_mull: RdLo and RdHi must be different registers.");

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	uint64_t result;

	if (sign) {
		result = (int64_t)r[rm] * (int64_t)r[rs];
	} else {
		result = (uint64_t)r[rm] * (uint64_t)r[rs];
	}

	r[rd_lo] = (uint32_t)result;
	r[rd_hi] = (uint32_t)(result >> 32);

	if (s) {
		N = (result >> 63) & 1;
		Z = (result == 0);
		UNAFFECTED(cf);
		UNAFFECTED(vf);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_mlal(bool s, bool sign, Register rd_lo, Register rd_hi,
                              Register rm, Register rs) {
	UNPREDICTABLE(rd_lo == PC || rd_hi == PC || rm == PC || rs == PC,
	              "arm_mlal: RdLo/RdHi/Rm or Rs must not be PC.");
	UNPREDICTABLE(rd_lo == rd_hi,
	              "arm_mlal: RdLo and RdHi must be different registers.");

	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	uint64_t result;
	uint64_t acc = ((uint64_t)(r[rd_hi]) << 32) | r[rd_lo];

	if (sign) {
		result = (int64_t)r[rm] * (int64_t)r[rs] + (int64_t)acc;
	} else {
		result = (uint64_t)r[rm] * (uint64_t)r[rs] + acc;
	}

	r[rd_lo] = (uint32_t)result;
	r[rd_hi] = (uint32_t)(result >> 32);

	if (s) {
		N = (result >> 63) & 1;
		Z = (result == 0);
		UNAFFECTED(C);
		UNAFFECTED(V);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_ldr(bool p, bool u, bool b, bool w, Register rn,
                             Register rd, reg_value_t offset) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	reg_value_t base = r[rn];
	reg_value_t addr = p ? base + (u ? offset : -offset) : base;

	LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "virtual address: 0x%X",
	                 addr);

	const void *mem = reinterpret_cast<const void *>(address_resolve_raw(addr));

	LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "resolved to: %p", mem);

	if (UNLIKELY(!mem)) {
		LAYER_DBE_LOG(*this, "%s", "error: resolved address is 0x00000000!");
		LAYER_DBE_SEND_PAUSED(*this);

		throw std::runtime_error("arm_ldr: resolved address is 0x00000000");
	}

	if (b) {
		memset(&r[rd], 0, sizeof(reg_value_t));
		memcpy(&r[rd], mem, sizeof(uint8_t));
	} else {
		memcpy(&r[rd], mem, sizeof(uint32_t));
	}

	LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "value read: 0x%X",
	                 r[rd]);

	if (w || !p) {
		UNPREDICTABLE(r[rn] == PC, "Writeback with PC as Rn.")

		r[rn] = base + (u ? offset : -offset);
		LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT,
		                 "wrote back to r%d: 0x%X", rn, r[rn]);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_str(bool p, bool u, bool b, bool w, Register rn,
                             Register rd, reg_value_t offset) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	reg_value_t base = r[rn];
	reg_value_t value = r[rd];
	reg_value_t addr = p ? base + (u ? offset : -offset) : base;

	/* SPECIAL CASE: When RD is PC, store ADDR + 12 */
	if (rd == PC) {
		value += 4;
	}

	LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "virtual address: 0x%X",
	                 addr);

	void *mem = reinterpret_cast<void *>(address_resolve_raw(addr));

	if (UNLIKELY(!mem)) {
		LAYER_DBE_LOG(*this, "%s", "error: resolved address is 0x00000000!");
		LAYER_DBE_SEND_PAUSED(*this);

		throw std::runtime_error("arm_str: resolved address is 0x00000000");
	}

	LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "resolved to: %p", mem);

	if (b) {
		memcpy(mem, &value, sizeof(uint8_t));
	} else {
		memcpy(mem, &value, sizeof(uint32_t));
	}

	LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT,
	                 "value wrote to %p: 0x%X", mem, value);

	if (w || !p) {
		UNPREDICTABLE(r[rn] == PC, "Writeback with PC as Rn.")

		r[rn] = base + (u ? offset : -offset);
		LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT,
		                 "wrote back to r%d: 0x%X", rn, r[rn]);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_ldrh(bool p, bool u, bool w, Register rn, Register rd,
                              uint8_t type, reg_value_t offset) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	reg_value_t base = r[rn];
	reg_value_t addr = p ? base + (u ? offset : -offset) : base;

	LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "virtual address: 0x%X",
	                 addr);

	const char *mem = reinterpret_cast<const char *>(address_resolve_raw(addr));

	LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "resolved to: %p", mem);

	if (UNLIKELY(!mem)) {
		LAYER_DBE_LOG(*this, "%s", "error: resolved address is 0x00000000!");
		LAYER_DBE_SEND_PAUSED(*this);

		throw std::runtime_error("arm_ldrh: resolved address is 0x00000000");
	}

	switch (type) {
	case 0b01: // LDRH
		r[rd] = 0;
		memcpy(&r[rd], mem, sizeof(uint16_t));
		break;

	case 0b10: // LDRSB
		int8_t byte;
		memcpy(&byte, mem, sizeof(int8_t));
		r[rd] = static_cast<reg_value_t>(static_cast<int32_t>(byte));
		break;

	case 0b11: // LDRSH
		int16_t word;
		memcpy(&word, mem, sizeof(int16_t));
		r[rd] = static_cast<reg_value_t>(static_cast<int32_t>(word));
		break;
	}

	LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "value read: 0x%X",
	                 r[rd]);

	if (w || !p) {
		UNPREDICTABLE(r[rn] == PC, "Writeback with PC as Rn.")

		r[rn] = base + (u ? offset : -offset);
		LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT,
		                 "wrote back to r%d: 0x%X", rn, r[rn]);
	}

	LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_strh(bool p, bool u, bool w, Register rn, Register rd,
                              uint8_t type, uint32_t offset) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	reg_value_t base = r[rn];
	reg_value_t value = r[rd];
	reg_value_t addr = p ? base + (u ? offset : -offset) : base;

	/* SPECIAL CASE: When RD is PC, it will actually store ADDR + 12. Since PC
	 * is always ADDR + 8, we just add 4 to it. */
	if (rd == PC) {
		value += 4;
	}

	LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "virtual address: 0x%X",
	                 addr);

	char *mem = reinterpret_cast<char *>(address_resolve_raw(addr));

	LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "resolved to: %p", mem);

	if (UNLIKELY(!mem)) {
		LAYER_DBE_LOG(*this, "%s", "error: resolved address is 0x00000000!");
		LAYER_DBE_SEND_PAUSED(*this);

		throw std::runtime_error("arm_strh: resolved address is 0x00000000");
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

void ExecutionState::arm_ldm(bool p, bool u, bool w, Register rn,
                             reg_value_t reg_list) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	reg_value_t base = r[rn];
	reg_value_t n = __builtin_popcount(reg_list);
	reg_value_t addr;

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

	const char *mem = reinterpret_cast<const char *>(address_resolve_raw(addr));

	LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "resolved to: %p", mem);

	if (UNLIKELY(!mem)) {
		LAYER_DBE_LOG(*this, "%s", "error: resolved address is 0x00000000!");
		LAYER_DBE_SEND_PAUSED(*this);

		throw std::runtime_error("arm_ldm: resolved address is 0x00000000");
	}

	for (reg_value_t i = 0; i < REG_COUNT; i++) {
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

void ExecutionState::arm_stm(bool p, bool u, bool w, Register rn,
                             reg_value_t reg_list) {
	LAYER_DBE_NEXT(*this, "%s: before", __func__);

	reg_value_t base = r[rn];
	reg_value_t n = __builtin_popcount(reg_list);
	reg_value_t addr;

	if (u) {
		addr = p ? base + 4 : base;
	} else {
		addr = p ? base - (n * 4) : base - 4;
	}

	LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "virtual address: 0x%X",
	                 addr);

	char *mem = reinterpret_cast<char *>(address_resolve_raw(addr));
	bool written = false;

	LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "resolved to: %p", mem);

	if (UNLIKELY(!mem)) {
		LAYER_DBE_LOG(*this, "%s", "error: resolved address is 0x00000000!");
		LAYER_DBE_SEND_PAUSED(*this);

		throw std::runtime_error("arm_ldm: resolved address is 0x00000000");
	}

	for (reg_value_t i = 0; i < REG_COUNT; i++) {
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
} // namespace layer
