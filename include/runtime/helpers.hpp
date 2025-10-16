#pragma once
#include <arch.hpp>

#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define UNPREDICTABLE(x, msg) static_assert(x, msg);
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
