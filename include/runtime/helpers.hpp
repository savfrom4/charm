#pragma once
#include <arch.hpp>

#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define UNPREDICTABLE(x, fmt, ...)                                             \
	if (UNLIKELY(x)) {                                                         \
		LAYER_DBE_LOG(*this, "UNPREDICTABLE: " fmt, __VA_ARGS__);              \
	}
#define UNAFFECTED(x)

#define EQ(x)                                                                  \
	if (Z) {                                                                   \
		x;                                                                     \
	}

#define NE(x)                                                                  \
	if (!Z) {                                                                  \
		x;                                                                     \
	}

#define CS(x)                                                                  \
	if (C) {                                                                   \
		x;                                                                     \
	}

#define CC(x)                                                                  \
	if (!C) {                                                                  \
		x;                                                                     \
	}

#define MI(x)                                                                  \
	if (N) {                                                                   \
		x;                                                                     \
	}

#define PL(x)                                                                  \
	if (!N) {                                                                  \
		x;                                                                     \
	}

#define VS(x)                                                                  \
	if (V) {                                                                   \
		x;                                                                     \
	}

#define VC(x)                                                                  \
	if (!V) {                                                                  \
		x;                                                                     \
	}

#define HI(x)                                                                  \
	if (C && !Z) {                                                             \
		x;                                                                     \
	}

#define LS(x)                                                                  \
	if (!C || Z) {                                                             \
		x;                                                                     \
	}

#define GE(x)                                                                  \
	if (N == V) {                                                              \
		x;                                                                     \
	}

#define LT(x)                                                                  \
	if (N != V) {                                                              \
		x;                                                                     \
	}

#define GT(x)                                                                  \
	if (!Z && (N == V)) {                                                      \
		x;                                                                     \
	}

#define LE(x)                                                                  \
	if (Z || (N != V)) {                                                       \
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

namespace charm::runtime {

constexpr inline reg_value_t op2_lsl(ExecutionState &ps, bool s,
                                     reg_value_t value, reg_value_t amount) {
	if (!amount)
		return value;

	if (amount > 32) {
		ps.C = false;
		return 0;
	}

	if (amount == 32) {
		ps.C = (value & 1) != 0; // bit 0
		return 0;
	}

	ps.C = (value & (1u << (32 - amount))) != 0; // last shifted bit
	return value << amount;
}

constexpr inline reg_value_t op2_lsr(ExecutionState &ps, bool s,
                                     reg_value_t value, reg_value_t amount) {
	if (!amount)
		return value;

	if (amount > 32) {
		ps.C = false;
		return 0;
	}

	if (amount == 32) {
		ps.C = (value & (1 << 31)) != 0; // bit 31
		return 0;
	}

	ps.C = (value & (1u << (amount - 1))) != 0; // last shifted bit
	return value >> amount;
}

constexpr inline reg_value_t op2_asr(ExecutionState &ps, bool s,
                                     reg_value_t value, reg_value_t amount) {
	if (!amount)
		return value;

	if (amount >= 32) {
		ps.C = (value & 0x80000000) != 0;
		return ps.C ? 0xFFFFFFFF : 0;
	}

	ps.C = (value & (1u << (amount - 1))) != 0; // last shifted bit
	return ((int32_t)value) >> amount;
}

constexpr inline reg_value_t op2_ror(ExecutionState &ps, bool s,
                                     reg_value_t value, reg_value_t amount) {
	if (!amount)
		return value;

	if (!(amount &= 0x1F)) {
		return value;
	}

	ps.C = (value & (1u << (amount - 1))) != 0; // last shifted bit
	return (value >> amount) | (value << (32 - amount));
}

} // namespace charm::runtime
