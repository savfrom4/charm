#include "liblayer/liblayer.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#define UNLIKELY(x) __builtin_expect(!!(x), 0)

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

inline void ExecutionState::arm_add(bool s, reg_idx_t rd, reg_idx_t rn,
                                    reg_value_t imm) {
  LAYER_DBE_STEPIN();

  if (s) {
    cs = __builtin_add_overflow(r[rn], imm, &r[rd]);
    int32_t unused;
    vs = __builtin_sadd_overflow(r[rn], imm, &unused);
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  } else {
    r[rd] = r[rn] + imm;
  }

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_adc(bool s, reg_idx_t rd, reg_idx_t rn,
                                    reg_value_t imm) {
  LAYER_DBE_STEPIN();

  reg_value_t operand = imm + cs;
  if (s) {
    cs = __builtin_add_overflow(r[rn], operand, &r[rd]);
    int32_t unused;
    vs = __builtin_sadd_overflow(r[rn], operand, &unused);
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  } else {
    r[rd] = r[rn] + operand;
  }

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_sub(bool s, reg_idx_t rd, reg_idx_t rn,
                                    reg_value_t imm) {
  LAYER_DBE_STEPIN();

  if (s) {
    cs = !__builtin_sub_overflow(r[rn], imm, &r[rd]);
    int32_t unused;
    vs = __builtin_ssub_overflow(r[rn], imm, &unused);
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  } else {
    r[rd] = r[rn] - imm;
  }

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_sbc(bool s, reg_idx_t rd, reg_idx_t rn,
                                    reg_value_t imm) {
  LAYER_DBE_STEPIN();

  reg_value_t operand = imm + !cs;
  if (s) {
    cs = !__builtin_sub_overflow(r[rn], operand, &r[rd]);
    int32_t unused;
    vs = __builtin_ssub_overflow(r[rn], operand, &unused);
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  } else {
    r[rd] = r[rn] - operand;
  }

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_cmp(bool s, reg_idx_t rd, reg_idx_t rn,
                                    reg_value_t imm) {
  LAYER_DBE_STEPIN();

  uint32_t result;
  cs = !__builtin_sub_overflow(r[rn], imm, &result);
  int32_t unused;
  vs = __builtin_ssub_overflow(r[rn], imm, &unused);
  mi = (result >> 31) & 1;
  z = !result;

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_mov(bool s, reg_idx_t rd, reg_idx_t rn,
                                    reg_value_t imm) {
  LAYER_DBE_STEPIN();

  r[rd] = imm;
  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_rsb(bool s, reg_idx_t rd, reg_idx_t rn,
                                    reg_value_t imm) {
  LAYER_DBE_STEPIN();

  if (s) {
    cs = !__builtin_sub_overflow(imm, r[rn], &r[rd]);
    int32_t unused;
    vs = __builtin_ssub_overflow(imm, r[rn], &unused);
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  } else {
    r[rd] = imm - r[rn];
  }

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_rsc(bool s, reg_idx_t rd, reg_idx_t rn,
                                    reg_value_t imm) {
  LAYER_DBE_STEPIN();

  reg_value_t operand = r[rn] + !cs;

  if (s) {
    cs = !__builtin_sub_overflow(imm, operand, &r[rd]);
    int32_t unused;
    vs = __builtin_ssub_overflow(imm, operand, &unused);
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  } else {
    r[rd] = imm - operand;
  }

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_and(bool s, reg_idx_t rd, reg_idx_t rn,
                                    reg_value_t imm) {
  LAYER_DBE_STEPIN();

  r[rd] = r[rn] & imm;

  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_eor(bool s, reg_idx_t rd, reg_idx_t rn,
                                    reg_value_t imm) {
  LAYER_DBE_STEPIN();

  r[rd] = r[rn] ^ imm;

  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_orr(bool s, reg_idx_t rd, reg_idx_t rn,
                                    reg_value_t imm) {
  LAYER_DBE_STEPIN();

  r[rd] = r[rn] | imm;
  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_bic(bool s, reg_idx_t rd, reg_idx_t rn,
                                    reg_value_t imm) {
  LAYER_DBE_STEPIN();

  r[rd] = r[rn] & ~imm;
  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_mvn(bool s, reg_idx_t rd, reg_idx_t rn,
                                    reg_value_t imm) {
  LAYER_DBE_STEPIN();

  r[rd] = ~imm;

  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_tst(bool s, reg_idx_t rd, reg_idx_t rn,
                                    reg_value_t imm) {
  LAYER_DBE_STEPIN();

  reg_value_t result = r[rn] & imm;
  mi = (result >> 31) & 1;
  z = !result;

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_teq(bool s, reg_idx_t rd, reg_idx_t rn,
                                    reg_value_t imm) {
  LAYER_DBE_STEPIN();

  reg_value_t result = r[rn] ^ imm;
  mi = (result >> 31) & 1;
  z = !result;

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_cmn(bool s, reg_idx_t rd, reg_idx_t rn,
                                    reg_value_t imm) {
  LAYER_DBE_STEPIN();

  reg_value_t result;
  cs = __builtin_add_overflow(r[rn], imm, &result);
  int32_t unused;
  vs = __builtin_sadd_overflow(r[rn], imm, &unused);
  mi = (result >> 31) & 1;
  z = !result;

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_mul(bool s, reg_idx_t rd, reg_idx_t rn,
                                    reg_idx_t rs, reg_idx_t rm) {
  LAYER_DBE_STEPIN();

  if (UNLIKELY(rd == rm)) {
    LAYER_DBE_LOG("Warning: UNPREDICTABLE: Rd and Rm must be different!");
  }

  r[rd] = r[rm] * r[rs];

  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_mla(bool s, reg_idx_t rd, reg_idx_t rn,
                                    reg_idx_t rs, reg_idx_t rm) {
  LAYER_DBE_STEPIN();

  if (UNLIKELY(rd == rm)) {
    LAYER_DBE_LOG("Warning: UNPREDICTABLE: Rd and Rm must be different!");
  }

  r[rd] = r[rm] * r[rs] + r[rn];
  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_mull(bool s, bool sign, reg_idx_t rd_lo,
                                     reg_idx_t rd_hi, reg_idx_t rm,
                                     reg_idx_t rs) {
  LAYER_DBE_STEPIN();

  uint64_t result;
  if (sign) {
    int64_t lhs = static_cast<int32_t>(r[rm]);
    int64_t rhs = static_cast<int32_t>(r[rs]);
    result = static_cast<uint64_t>(lhs * rhs);
  } else {
    result = static_cast<uint64_t>(r[rm]) * static_cast<uint64_t>(r[rs]);
  }

  r[rd_lo] = static_cast<uint32_t>(result);
  r[rd_hi] = static_cast<uint32_t>(result >> 32);

  if (s) {
    mi = (result >> 63) & 1;
    z = (result == 0);
  }

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_mlal(bool s, bool sign, reg_idx_t rd_lo,
                                     reg_idx_t rd_hi, reg_idx_t rm,
                                     reg_idx_t rs) {
  LAYER_DBE_STEPIN();

  uint64_t result;
  if (sign) {
    int64_t lhs = static_cast<int32_t>(r[rm]);
    int64_t rhs = static_cast<int32_t>(r[rs]);
    result = static_cast<uint64_t>(lhs * rhs);
  } else {
    result = static_cast<uint64_t>(r[rm]) * static_cast<uint64_t>(r[rs]);
  }

  uint64_t acc = (static_cast<uint64_t>(r[rd_hi]) << 32) | r[rd_lo];
  acc += result;

  r[rd_lo] = static_cast<uint32_t>(acc);
  r[rd_hi] = static_cast<uint32_t>(acc >> 32);

  if (s) {
    mi = (acc >> 63) & 1;
    z = (acc == 0);
  }

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_ldr(bool pre_indx, bool add, bool byte,
                                    bool write_back, reg_idx_t rn, reg_idx_t rd,
                                    reg_value_t offset) {
  LAYER_DBE_STEPIN();

  reg_value_t base = r[rn];
  reg_value_t addr = pre_indx ? base + (add ? offset : -offset) : base;

  LAYER_DBE_LOG("Note: virtual address: 0x%X", addr);

  const void *mem = reinterpret_cast<const void *>(address_resolve(addr));

  LAYER_DBE_LOG("Note: resolved to: %p", mem);

  if (UNLIKELY(!mem)) {
    LAYER_DBE_LOG("Error: resolved address is 0x00000000!");
    LAYER_DBE_BREAK();

    throw std::runtime_error("arm_ldr: resolved address is 0x00000000");
  }

  if (byte) {
    memset(&r[rd], 0, sizeof(reg_value_t));
    memcpy(&r[rd], mem, sizeof(uint8_t));
  } else {
    memcpy(&r[rd], mem, sizeof(uint32_t));
  }

  LAYER_DBE_LOG("Note: value read: 0x%x", r[rd]);

  if (write_back || !pre_indx) {
    // SPECIAL CASE: write-back to PC is UNPREDICTABLE, catch that
    if (UNLIKELY(r[rn] == PC)) {
      LAYER_DBE_LOG("UNPREDICTABLE: writeback to PC as Rn is not allowed!");
    }

    r[rn] = base + (add ? offset : -offset);
    LAYER_DBE_LOG("Note: wrote back to r%d: 0x%x", rn, r[rn]);
  }

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_str(bool pre_indx, bool add, bool byte,
                                    bool write_back, reg_idx_t rn, reg_idx_t rd,
                                    reg_value_t offset) {
  LAYER_DBE_STEPIN();

  reg_value_t base = r[rn];
  reg_value_t value = r[rd];
  reg_value_t addr = pre_indx ? base + (add ? offset : -offset) : base;

  /* SPECIAL CASE: When RD is PC, it will actually store ADDR + 12. Since PC is
   * always ADDR + 8, we just add 4 to it. */
  if (rd == PC) {
    value += 4;
  }

  LAYER_DBE_LOG("Note: virtual address: 0x%X", addr);

  void *mem = reinterpret_cast<void *>(address_resolve(addr));

  if (UNLIKELY(!mem)) {
    LAYER_DBE_LOG("Error: resolved address is 0x00000000!");
    LAYER_DBE_BREAK();

    throw std::runtime_error("arm_str: resolved address is 0x00000000");
  }

  LAYER_DBE_LOG("Note: resolved to: %p", mem);

  if (byte) {
    memcpy(mem, &value, sizeof(uint8_t));
  } else {
    memcpy(mem, &value, sizeof(uint32_t));
  }

  LAYER_DBE_LOG("Note: value wrote to %p: 0x%x", mem, value);

  if (write_back || !pre_indx) {
    // SPECIAL CASE: write-back to PC is UNPREDICTABLE, catch that
    if (UNLIKELY(r[rn] == PC)) {
      LAYER_DBE_LOG("UNPREDICTABLE: writeback to PC as Rn is not allowed!");
    }

    r[rn] = base + (add ? offset : -offset);
    LAYER_DBE_LOG("Note: wrote back to r%d: 0x%x", rn, r[rn]);
  }

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_ldrh(bool pre_indx, bool add, bool write_back,
                                     reg_idx_t rn, reg_idx_t rd, uint8_t type,
                                     uint32_t offset) {
  LAYER_DBE_STEPIN();

  reg_value_t base = r[rn];
  reg_value_t addr = pre_indx ? base + (add ? offset : -offset) : base;

  LAYER_DBE_LOG("Note: virtual address: 0x%X", addr);

  const char *mem = reinterpret_cast<const char *>(address_resolve(addr));

  LAYER_DBE_LOG("Note: resolved to: %p", mem);

  if (UNLIKELY(!mem)) {
    LAYER_DBE_LOG("Error: resolved address is 0x00000000!");
    LAYER_DBE_BREAK();

    throw std::runtime_error("arm_ldrh: resolved address is 0x00000000");
  }

  switch (type) {
  case 0b00:
    LAYER_DBE_LOG("Error: SWP is not implemented!");
    LAYER_DBE_BREAK();
    throw std::runtime_error("arm_ldrh: SWP is not implemented!");

  case 0b01: // LDRHR
    r[rd] = 0;
    memcpy(&r[rd], mem, sizeof(uint16_t));
    break;

  case 0b10: // LDRSBR
    int8_t byte;
    memcpy(&byte, mem, sizeof(int8_t));
    r[rd] = static_cast<reg_value_t>(static_cast<int32_t>(byte));
    break;

  case 0b11: // LDRSHR
    int16_t word;
    memcpy(&word, mem, sizeof(int16_t));
    r[rd] = static_cast<reg_value_t>(static_cast<int32_t>(word));
    break;
  }

  LAYER_DBE_LOG("Note: value read: 0x%x", r[rd]);

  if (write_back || !pre_indx) {
    // SPECIAL CASE: write-back to PC is UNPREDICTABLE, catch that
    if (UNLIKELY(r[rn] == PC)) {
      LAYER_DBE_LOG("UNPREDICTABLE: writeback to PC as Rn is not allowed!");
    }

    r[rn] = base + (add ? offset : -offset);
    LAYER_DBE_LOG("Note: wrote back to r%d: 0x%x", rn, r[rn]);
  }

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_strh(bool pre_indx, bool add, bool write_back,
                                     reg_idx_t rn, reg_idx_t rd, uint8_t type,
                                     uint32_t offset) {
  LAYER_DBE_STEPIN();

  reg_value_t base = r[rn];
  reg_value_t value = r[rd];
  reg_value_t addr = pre_indx ? base + (add ? offset : -offset) : base;

  /* SPECIAL CASE: When RD is PC, it will actually store ADDR + 12. Since PC
   * is always ADDR + 8, we just add 4 to it. */
  if (rd == PC) {
    value += 4;
  }

  LAYER_DBE_LOG("Note: virtual address: 0x%X", addr);

  char *mem = reinterpret_cast<char *>(address_resolve(addr));

  LAYER_DBE_LOG("Note: resolved to: %p", mem);

  if (UNLIKELY(!mem)) {
    LAYER_DBE_LOG("Error: resolved address is 0x00000000!");
    LAYER_DBE_BREAK();

    throw std::runtime_error("arm_strh: resolved address is 0x00000000");
  }

  switch (type) {
  case 0b00:
    LAYER_DBE_LOG("Error: SWP is not implemented!");
    LAYER_DBE_BREAK();
    throw std::runtime_error("arm_strh: SWP is not implemented!");

  case 0b01: // STRHR
    memcpy(mem, &value, sizeof(uint16_t));
    break;

  case 0b10: // STRSBR
    memcpy(mem, &value, sizeof(int8_t));
    break;

  case 0b11: // STRSHR
    memcpy(mem, &value, sizeof(int16_t));
    break;
  }

  LAYER_DBE_LOG("Note: value stored to %p: 0x%x", mem, r[rd]);

  if (write_back || !pre_indx) {
    // SPECIAL CASE: write-back to PC is UNPREDICTABLE, catch that
    if (UNLIKELY(r[rn] == PC)) {
      LAYER_DBE_LOG("UNPREDICTABLE: writeback to PC as Rn is not allowed!");
    }

    r[rn] = base + (add ? offset : -offset);
    LAYER_DBE_LOG("Note: wrote back to r%d: 0x%x", rn, r[rn]);
  }

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_ldm(bool pre_indx, bool add, bool write_back,
                                    reg_idx_t rn, reg_value_t reg_list) {
  LAYER_DBE_STEPIN();

  reg_value_t base = r[rn];
  reg_value_t n = __builtin_popcount(reg_list);
  reg_value_t addr;

  if (add) {
    addr = pre_indx ? base + 4 : base;
  } else {
    addr = pre_indx ? base - (n * 4) : base - 4;
  }

  if (write_back) {
    r[rn] = add ? base + n * 4 : base - n * 4;
    LAYER_DBE_LOG("Note: wrote back to r%d: 0x%x", rn, r[rn]);
  }

  LAYER_DBE_LOG("Note: virtual address: 0x%X", addr);

  const char *mem = reinterpret_cast<const char *>(address_resolve(addr));

  LAYER_DBE_LOG("Note: resolved to: %p", mem);

  if (UNLIKELY(!mem)) {
    LAYER_DBE_LOG("Error: resolved address is 0x00000000!");
    LAYER_DBE_BREAK();

    throw std::runtime_error("arm_ldm: resolved address is 0x00000000");
  }

  for (reg_value_t i = 0; i < REG_COUNT; i++) {
    if (!((reg_list >> i) & 1)) {
      continue;
    }

    memcpy(&r[i], mem, sizeof(uint32_t));
    LAYER_DBE_LOG("Note: value read from %p: 0x%x", mem, r[i]);
    LAYER_DBE_STEPIN();
    mem += 4;
  }

  LAYER_DBE_STEPIN();
}

inline void ExecutionState::arm_stm(bool pre_indx, bool add, bool write_back,
                                    reg_idx_t rn, reg_value_t reg_list) {
  LAYER_DBE_STEPIN();

  reg_value_t base = r[rn];
  reg_value_t n = __builtin_popcount(reg_list);
  reg_value_t addr;

  if (add) {
    addr = pre_indx ? base + 4 : base;
  } else {
    addr = pre_indx ? base - n * 4 : base - 4;
  }

  LAYER_DBE_LOG("Note: virtual address: 0x%X", addr);

  char *mem = reinterpret_cast<char *>(address_resolve(addr));
  bool written = false;

  LAYER_DBE_LOG("Note: resolved to: %p", mem);

  if (UNLIKELY(!mem)) {
    LAYER_DBE_LOG("Error: resolved address is 0x00000000!");
    LAYER_DBE_BREAK();

    throw std::runtime_error("arm_ldm: resolved address is 0x00000000");
  }

  for (reg_value_t i = 0; i < REG_COUNT; i++) {
    if (!((reg_list >> i) & 1)) {
      continue;
    }

    memcpy(mem, &r[i], sizeof(uint32_t));
    LAYER_DBE_LOG("Note: value wrote to %p: 0x%x", mem, r[i]);
    LAYER_DBE_STEPIN();
    mem += 4;

    if (!write_back || written) {
      continue;
    }

    // We write-back now
    r[rn] = add ? base + n * 4 : base - n * 4;
    LAYER_DBE_LOG("Note: wrote back to r%d: 0x%x", rn, r[rn]);
    written = true;
  }

  LAYER_DBE_STEPIN();
}

} // namespace layer
