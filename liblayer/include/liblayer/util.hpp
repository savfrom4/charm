#pragma once
#include "execution_state.hpp"

#define EQ(x)                                                                  \
  if (ps.z) {                                                                  \
    x;                                                                         \
  }

#define NE(x)                                                                  \
  if (!ps.z) {                                                                 \
    x;                                                                         \
  }

#define CS(x)                                                                  \
  if (ps.cs) {                                                                 \
    x;                                                                         \
  }

#define CC(x)                                                                  \
  if (!ps.cs) {                                                                \
    x;                                                                         \
  }

#define MI(x)                                                                  \
  if (ps.mi) {                                                                 \
    x;                                                                         \
  }

#define PL(x)                                                                  \
  if (!ps.mi) {                                                                \
    x;                                                                         \
  }

#define VS(x)                                                                  \
  if (ps.vs) {                                                                 \
    x;                                                                         \
  }

#define VC(x)                                                                  \
  if (!ps.vs) {                                                                \
    x;                                                                         \
  }

#define HI(x)                                                                  \
  if (ps.cs && !ps.z) {                                                        \
    x;                                                                         \
  }

#define LS(x)                                                                  \
  if (!ps.cs || ps.z) {                                                        \
    x;                                                                         \
  }

#define GE(x)                                                                  \
  if (ps.mi == ps.vs) {                                                        \
    x;                                                                         \
  }

#define LT(x)                                                                  \
  if (ps.mi != ps.vs) {                                                        \
    x;                                                                         \
  }

#define GT(x)                                                                  \
  if (!ps.z && (ps.mi == ps.vs)) {                                             \
    x;                                                                         \
  }

#define LE(x)                                                                  \
  if (ps.z || (ps.mi != ps.vs)) {                                              \
    x;                                                                         \
  }

#define AL(x)                                                                  \
  if (1) {                                                                     \
    x;                                                                         \
  }

#define NV(x)                                                                  \
  if (0) {                                                                     \
    x;                                                                         \
  }

namespace layer {

constexpr inline reg_value_t op2_lsl(ExecutionState &ps, bool s,
                                     reg_value_t value, reg_value_t amount) {
  if (!amount)
    return value;

  if (amount > 32) {
    ps.cs = false;
    return 0;
  }

  if (amount == 32) {
    ps.cs = (value & 1) != 0; // bit 0
    return 0;
  }

  ps.cs = (value & (1u << (32 - amount))) != 0; // last shifted bit
  return value << amount;
}

constexpr inline reg_value_t op2_lsr(ExecutionState &ps, bool s,
                                     reg_value_t value, reg_value_t amount) {
  if (!amount)
    return value;

  if (amount > 32) {
    ps.cs = false;
    return 0;
  }

  if (amount == 32) {
    ps.cs = (value & (1 << 31)) != 0; // bit 31
    return 0;
  }

  ps.cs = (value & (1u << (amount - 1))) != 0; // last shifted bit
  return value >> amount;
}

constexpr inline reg_value_t op2_asr(ExecutionState &ps, bool s,
                                     reg_value_t value, reg_value_t amount) {
  if (!amount)
    return value;

  if (amount >= 32) {
    ps.cs = (value & 0x80000000) != 0;
    return ps.cs ? 0xFFFFFFFF : 0;
  }

  ps.cs = (value & (1u << (amount - 1))) != 0; // last shifted bit
  return ((int32_t)value) >> amount;
}

constexpr inline reg_value_t op2_ror(ExecutionState &ps, bool s,
                                     reg_value_t value, reg_value_t amount) {
  if (!amount)
    return value;

  if (!(amount &= 0x1F)) {
    return value;
  }

  ps.cs = (value & (1u << (amount - 1))) != 0; // last shifted bit
  return (value >> amount) | (value << (32 - amount));
}

} // namespace layer
