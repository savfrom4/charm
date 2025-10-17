#pragma once
#include <arch.hpp>

#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define UNPREDICTABLE(x, msg)
#define UNAFFECTED(x)

#define EQ(x)                                                                  \
	if (cpu.Z) {                                                               \
		x;                                                                     \
	}

#define NE(x)                                                                  \
	if (!cpu.Z) {                                                              \
		x;                                                                     \
	}

#define CS(x)                                                                  \
	if (cpu.C) {                                                               \
		x;                                                                     \
	}

#define CC(x)                                                                  \
	if (!cpu.C) {                                                              \
		x;                                                                     \
	}

#define MI(x)                                                                  \
	if (cpu.N) {                                                               \
		x;                                                                     \
	}

#define PL(x)                                                                  \
	if (!cpu.N) {                                                              \
		x;                                                                     \
	}

#define VS(x)                                                                  \
	if (cpu.V) {                                                               \
		x;                                                                     \
	}

#define VC(x)                                                                  \
	if (!cpu.V) {                                                              \
		x;                                                                     \
	}

#define HI(x)                                                                  \
	if (cpu.C && !cpu.Z) {                                                     \
		x;                                                                     \
	}

#define LS(x)                                                                  \
	if (!cpu.C || cpu.Z) {                                                     \
		x;                                                                     \
	}

#define GE(x)                                                                  \
	if (cpu.N == cpu.V) {                                                      \
		x;                                                                     \
	}

#define LT(x)                                                                  \
	if (cpu.N != cpu.V) {                                                      \
		x;                                                                     \
	}

#define GT(x)                                                                  \
	if (!cpu.Z && (cpu.N == cpu.V)) {                                          \
		x;                                                                     \
	}

#define LE(x)                                                                  \
	if (cpu.Z || (cpu.N != cpu.V)) {                                           \
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
