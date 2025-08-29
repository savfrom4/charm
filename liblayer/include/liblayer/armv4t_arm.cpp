#include "liblayer.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>

constexpr inline reg_value_t op2_lsl(reg_value_t value, reg_value_t amount) {
  if (!amount)
    return value;

  return (amount >= 32) ? 0 : value << amount;
}

constexpr inline reg_value_t op2_lsr(reg_value_t value, reg_value_t amount) {
  if (!amount)
    return value;

  return (amount >= 32) ? 0 : value >> amount;
}

constexpr inline reg_value_t op2_asr(reg_value_t value, reg_value_t amount) {
  if (!amount)
    return value;

  if (amount >= 32)
    return (value & 0x80000000) ? 0xFFFFFFFF : 0;
  return ((int32_t)value) >> amount;
}

constexpr inline reg_value_t op2_ror(reg_value_t value, reg_value_t amount) {
  if (!amount)
    return value;

  amount &= 0x1F;
  if (amount == 0) {
    return value;
  }
  return (value >> amount) | (value << (32 - amount));
}

[[gnu::always_inline]]
inline void ProgramState::arm_add(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_add: before r" << static_cast<int>(rd) << "=" << std::hex
                                << r[rd] << ", r" << static_cast<int>(rn) << "="
                                << r[rn] << ", imm=" << imm << std::dec);
  if (s) {
    cs = __builtin_add_overflow(r[rn], imm, &r[rd]);
    int32_t unused;
    vs = __builtin_sadd_overflow(r[rn], imm, &unused);
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  } else {
    r[rd] = r[rn] + imm;
  }
  DEBUG_LOG("arm_add: after r" << static_cast<int>(rd) << "=" << std::hex
                               << r[rd] << std::dec);
}

[[gnu::always_inline]]
inline void ProgramState::arm_adc(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_adc: before r" << static_cast<int>(rd) << "=" << std::hex
                                << r[rd] << ", r" << static_cast<int>(rn) << "="
                                << r[rn] << ", imm=" << imm << ", cs=" << cs
                                << std::dec);
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
  DEBUG_LOG("arm_adc: after r" << static_cast<int>(rd) << "=" << std::hex
                               << r[rd] << std::dec);
}

[[gnu::always_inline]]
inline void ProgramState::arm_sub(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_sub: before r" << static_cast<int>(rd) << "=" << std::hex
                                << r[rd] << ", r" << static_cast<int>(rn) << "="
                                << r[rn] << ", imm=" << imm << std::dec);
  if (s) {
    cs = !__builtin_sub_overflow(r[rn], imm, &r[rd]);
    int32_t unused;
    vs = __builtin_ssub_overflow(r[rn], imm, &unused);
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  } else {
    r[rd] = r[rn] - imm;
  }
  DEBUG_LOG("arm_sub: after r" << static_cast<int>(rd) << "=" << std::hex
                               << r[rd] << std::dec);
}

[[gnu::always_inline]]
inline void ProgramState::arm_sbc(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_sbc: before r" << static_cast<int>(rd) << "=" << std::hex
                                << r[rd] << ", r" << static_cast<int>(rn) << "="
                                << r[rn] << ", imm=" << imm << ", cs=" << cs
                                << std::dec);
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
  DEBUG_LOG("arm_sbc: after r" << static_cast<int>(rd) << "=" << std::hex
                               << r[rd] << std::dec);
}

[[gnu::always_inline]]
inline void ProgramState::arm_cmp(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_cmp: before r" << static_cast<int>(rn) << "=" << std::hex
                                << r[rn] << ", imm=" << imm << std::dec);
  uint32_t result;
  cs = !__builtin_sub_overflow(r[rn], imm, &result);
  int32_t unused;
  vs = __builtin_ssub_overflow(r[rn], imm, &unused);
  mi = (result >> 31) & 1;
  z = !result;
  DEBUG_LOG("arm_cmp: result=" << std::hex << result << " (flags: N=" << mi
                               << ", Z=" << z << ", C=" << cs << ", V=" << vs
                               << ")" << std::dec);
}

[[gnu::always_inline]]
inline void ProgramState::arm_mov(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_mov: before r" << static_cast<int>(rd) << "=" << std::hex
                                << r[rd] << ", imm=" << imm << std::dec);
  r[rd] = imm;
  DEBUG_LOG("arm_mov: after r" << static_cast<int>(rd) << "=" << std::hex
                               << r[rd] << std::dec);
  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }
}

[[gnu::always_inline]]
inline void ProgramState::arm_rsb(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_rsb: before r" << static_cast<int>(rd) << "=" << std::hex
                                << r[rd] << ", r" << static_cast<int>(rn) << "="
                                << r[rn] << ", imm=" << imm << std::dec);
  if (s) {
    cs = !__builtin_sub_overflow(imm, r[rn], &r[rd]);
    int32_t unused;
    vs = __builtin_ssub_overflow(imm, r[rn], &unused);
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  } else {
    r[rd] = imm - r[rn];
  }
  DEBUG_LOG("arm_rsb: after r" << static_cast<int>(rd) << "=" << std::hex
                               << r[rd] << std::dec);
}

[[gnu::always_inline]]
inline void ProgramState::arm_rsc(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_rsc: before r" << static_cast<int>(rd) << "=" << std::hex
                                << r[rd] << ", r" << static_cast<int>(rn) << "="
                                << r[rn] << ", imm=" << imm << ", cs=" << cs
                                << std::dec);
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
  DEBUG_LOG("arm_rsc: after r" << static_cast<int>(rd) << "=" << std::hex
                               << r[rd] << std::dec);
}

[[gnu::always_inline]]
inline void ProgramState::arm_and(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_and: before r" << static_cast<int>(rd) << "=" << std::hex
                                << r[rd] << ", r" << static_cast<int>(rn) << "="
                                << r[rn] << ", imm=" << imm << std::dec);
  r[rd] = r[rn] & imm;
  DEBUG_LOG("arm_and: after r" << static_cast<int>(rd) << "=" << std::hex
                               << r[rd] << std::dec);
  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }
}

[[gnu::always_inline]]
inline void ProgramState::arm_eor(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_eor: before r" << static_cast<int>(rd) << "=" << std::hex
                                << r[rd] << ", r" << static_cast<int>(rn) << "="
                                << r[rn] << ", imm=" << imm << std::dec);
  r[rd] = r[rn] ^ imm;
  DEBUG_LOG("arm_eor: after r" << static_cast<int>(rd) << "=" << std::hex
                               << r[rd] << std::dec);
  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }
}

[[gnu::always_inline]]
inline void ProgramState::arm_orr(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_orr: before r" << static_cast<int>(rd) << "=" << std::hex
                                << r[rd] << ", r" << static_cast<int>(rn) << "="
                                << r[rn] << ", imm=" << imm << std::dec);
  r[rd] = r[rn] | imm;
  DEBUG_LOG("arm_orr: after r" << static_cast<int>(rd) << "=" << std::hex
                               << r[rd] << std::dec);
  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }
}

[[gnu::always_inline]]
inline void ProgramState::arm_bic(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_bic: before r" << static_cast<int>(rd) << "=" << std::hex
                                << r[rd] << ", r" << static_cast<int>(rn) << "="
                                << r[rn] << ", imm=" << imm << std::dec);
  r[rd] = r[rn] & ~imm;
  DEBUG_LOG("arm_bic: after r" << static_cast<int>(rd) << "=" << std::hex
                               << r[rd] << std::dec);
  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }
}

[[gnu::always_inline]]
inline void ProgramState::arm_mvn(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_mvn: before r" << static_cast<int>(rd) << "=" << std::hex
                                << r[rd] << ", imm=" << imm << std::dec);
  r[rd] = ~imm;
  DEBUG_LOG("arm_mvn: after r" << static_cast<int>(rd) << "=" << std::hex
                               << r[rd] << std::dec);
  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }
}

[[gnu::always_inline]]
inline void ProgramState::arm_tst(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_tst: before r" << static_cast<int>(rn) << "=" << std::hex
                                << r[rn] << ", imm=" << imm << std::dec);
  reg_value_t result = r[rn] & imm;
  mi = (result >> 31) & 1;
  z = !result;
  DEBUG_LOG("arm_tst: result=" << std::hex << result << " (flags: N=" << mi
                               << ", Z=" << z << ")" << std::dec);
}

[[gnu::always_inline]]
inline void ProgramState::arm_teq(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_teq: before r" << static_cast<int>(rn) << "=" << std::hex
                                << r[rn] << ", imm=" << imm << std::dec);
  reg_value_t result = r[rn] ^ imm;
  mi = (result >> 31) & 1;
  z = !result;
  DEBUG_LOG("arm_teq: result=" << std::hex << result << " (flags: N=" << mi
                               << ", Z=" << z << ")" << std::dec);
}

[[gnu::always_inline]]
inline void ProgramState::arm_cmn(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_cmn: before r" << static_cast<int>(rn) << "=" << std::hex
                                << r[rn] << ", imm=" << imm << std::dec);
  reg_value_t result;
  cs = __builtin_add_overflow(r[rn], imm, &result);
  int32_t unused;
  vs = __builtin_sadd_overflow(r[rn], imm, &unused);
  mi = (result >> 31) & 1;
  z = !result;
  DEBUG_LOG("arm_cmn: result=" << std::hex << result << " (flags: N=" << mi
                               << ", Z=" << z << ", C=" << cs << ", V=" << vs
                               << ")" << std::dec);
}

[[gnu::always_inline]]
inline void ProgramState::arm_mul(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_idx_t rs, reg_idx_t rm) {
  DEBUG_LOG("arm_mul: before r" << static_cast<int>(rd) << "=" << std::hex
                                << r[rd] << ", r" << static_cast<int>(rm) << "="
                                << r[rm] << ", r" << static_cast<int>(rs) << "="
                                << r[rs] << std::dec);
  r[rd] = r[rm] * r[rs];
  DEBUG_LOG("arm_mul: after r" << static_cast<int>(rd) << "=" << std::hex
                               << r[rd] << std::dec);
  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }
}

[[gnu::always_inline]]
inline void ProgramState::arm_mla(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_idx_t rs, reg_idx_t rm) {
  DEBUG_LOG("arm_mla: before r" << static_cast<int>(rd) << "=" << std::hex
                                << r[rd] << ", r" << static_cast<int>(rm) << "="
                                << r[rm] << ", r" << static_cast<int>(rs) << "="
                                << r[rs] << ", r" << static_cast<int>(rn) << "="
                                << r[rn] << std::dec);
  r[rd] = r[rm] * r[rs] + r[rn];
  DEBUG_LOG("arm_mla: after r" << static_cast<int>(rd) << "=" << std::hex
                               << r[rd] << std::dec);
  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }
}

[[gnu::always_inline]]
inline void ProgramState::arm_mull(bool s, bool sign, reg_idx_t rd_lo,
                                   reg_idx_t rd_hi, reg_idx_t rm,
                                   reg_idx_t rs) {
  DEBUG_LOG("arm_mull: before r"
            << static_cast<int>(rd_lo) << "=" << std::hex << r[rd_lo] << ", r"
            << static_cast<int>(rd_hi) << "=" << r[rd_hi] << ", r"
            << static_cast<int>(rm) << "=" << r[rm] << ", r"
            << static_cast<int>(rs) << "=" << r[rs] << std::dec);
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

  DEBUG_LOG("arm_mull: result="
            << std::hex << std::setfill('0') << std::setw(16) << result << " (r"
            << static_cast<int>(rd_lo) << "=" << std::hex << r[rd_lo] << ", r"
            << static_cast<int>(rd_hi) << "=" << r[rd_hi] << ")" << std::dec);

  if (s) {
    mi = (result >> 63) & 1;
    z = (result == 0);
  }
}

[[gnu::always_inline]]
inline void ProgramState::arm_mlal(bool s, bool sign, reg_idx_t rd_lo,
                                   reg_idx_t rd_hi, reg_idx_t rm,
                                   reg_idx_t rs) {
  DEBUG_LOG("arm_mlal: before r"
            << static_cast<int>(rd_lo) << "=" << std::hex << r[rd_lo] << ", r"
            << static_cast<int>(rd_hi) << "=" << r[rd_hi] << ", r"
            << static_cast<int>(rm) << "=" << r[rm] << ", r"
            << static_cast<int>(rs) << "=" << r[rs] << std::dec);
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

  DEBUG_LOG("arm_mlal: result="
            << std::hex << std::setfill('0') << std::setw(16) << acc << " (r"
            << static_cast<int>(rd_lo) << "=" << std::hex << r[rd_lo] << ", r"
            << static_cast<int>(rd_hi) << "=" << r[rd_hi] << ")" << std::dec);

  if (s) {
    mi = (acc >> 63) & 1;
    z = (acc == 0);
  }
}

[[gnu::always_inline]]
inline void ProgramState::arm_ldr(bool pre_indx, bool add, bool byte,
                                  bool write_back, reg_idx_t rn, reg_idx_t rd,
                                  reg_value_t offset, bool copy) {
  reg_value_t base = r[rn];
  reg_value_t addr = pre_indx ? base + (add ? offset : -offset) : base;

  DEBUG_LOG("arm_ldr: r" << static_cast<int>(rn) << ", r"
                         << static_cast<int>(rd) << ", offset=" << offset
                         << ", pre_indx=" << pre_indx << ", add=" << add
                         << ", byte=" << byte << ", write_back=" << write_back
                         << ", addr=" << std::hex << addr << std::dec);

  if (copy) {
    const void *mem = reinterpret_cast<const void *>(address_resolve(addr));

    if (byte) {
      memset(&r[rd], 0, sizeof(reg_value_t));
      memcpy(&r[rd], mem, sizeof(uint8_t));
    } else {
      memcpy(&r[rd], mem, sizeof(uint32_t));
    }
    DEBUG_LOG("arm_ldr: read value=" << std::hex << r[rd] << std::dec);
  }

  if (write_back) {
    r[rn] = pre_indx ? addr : base + (add ? offset : -offset);
    DEBUG_LOG("arm_ldr: write back to r" << static_cast<int>(rn) << "="
                                         << std::hex << r[rn] << std::dec);
  }
}

[[gnu::always_inline]]
inline void ProgramState::arm_str(bool pre_indx, bool add, bool byte,
                                  bool write_back, reg_idx_t rn, reg_idx_t rd,
                                  reg_value_t offset, bool copy) {
  reg_value_t base = r[rn];
  reg_value_t addr = pre_indx ? base + (add ? offset : -offset) : base;

  DEBUG_LOG("arm_str: r" << static_cast<int>(rn) << ", r"
                         << static_cast<int>(rd) << ", offset=" << offset
                         << ", pre_indx=" << pre_indx << ", add=" << add
                         << ", byte=" << byte << ", write_back=" << write_back
                         << ", addr=" << std::hex << addr << std::dec);

  if (copy) {
    void *mem = reinterpret_cast<void *>(address_resolve(addr));

    if (byte) {
      memcpy(mem, &r[rd], sizeof(uint8_t));
      DEBUG_LOG("arm_str: wrote value=" << std::hex
                                        << (static_cast<uint32_t>(r[rd] & 0xFF))
                                        << " to address " << addr << std::dec);
    } else {
      memcpy(mem, &r[rd], sizeof(uint32_t));
      DEBUG_LOG("arm_str: wrote value=" << std::hex << r[rd] << " to address "
                                        << addr << std::dec);
    }
  }

  if (write_back) {
    r[rn] = pre_indx ? addr : base + (add ? offset : -offset);
    DEBUG_LOG("arm_str: write back to r" << static_cast<int>(rn) << "="
                                         << std::hex << r[rn] << std::dec);
  }
}

[[gnu::always_inline]]
inline void ProgramState::arm_ldm(bool pre_indx, bool add, bool write_back,
                                  reg_idx_t rn, reg_value_t reg_list,
                                  bool copy) {
  reg_value_t base = r[rn];
  reg_value_t n = __builtin_popcount(reg_list);
  reg_value_t addr;

  if (add) {
    addr = pre_indx ? base + 4 : base;
  } else {
    addr = pre_indx ? base - (n * 4) : base - 4;
  }

  DEBUG_LOG("arm_ldm: r" << static_cast<int>(rn) << ", reg_list=" << std::hex
                         << reg_list << ", pre_indx=" << pre_indx
                         << ", add=" << add << ", write_back=" << write_back
                         << ", starting addr=" << addr << std::dec);

  if (copy) {
    const char *mem = reinterpret_cast<const char *>(address_resolve(addr));
    for (reg_value_t i = 0; i < REG_COUNT; i++) {
      if (!((reg_list >> i) & 1)) {
        continue;
      }

      memcpy(&r[i], mem, sizeof(uint32_t));
      DEBUG_LOG("arm_ldm: read r" << static_cast<int>(i) << "=" << std::hex
                                  << r[i] << " from addr=" << std::hex
                                  << reinterpret_cast<uintptr_t>(mem)
                                  << std::dec);
      mem += 4;
    }
  }

  if (write_back) {
    r[rn] = add ? base + n * 4 : base - n * 4;
    DEBUG_LOG("arm_ldm: write back to r" << static_cast<int>(rn) << "="
                                         << std::hex << r[rn] << std::dec);
  }
}

[[gnu::always_inline]]
inline void ProgramState::arm_stm(bool pre_indx, bool add, bool write_back,
                                  reg_idx_t rn, reg_value_t reg_list,
                                  bool copy) {
  reg_value_t base = r[rn];
  reg_value_t n = __builtin_popcount(reg_list);
  reg_value_t addr;

  if (add) {
    addr = pre_indx ? base + 4 : base;
  } else {
    addr = pre_indx ? base - (n * 4) : base - 4;
  }

  DEBUG_LOG("arm_stm: r" << static_cast<int>(rn) << ", reg_list=" << std::hex
                         << reg_list << ", pre_indx=" << pre_indx
                         << ", add=" << add << ", write_back=" << write_back
                         << ", starting addr=" << addr << std::dec);

  if (copy) {
    char *mem = reinterpret_cast<char *>(address_resolve(addr));
    for (reg_value_t i = 0; i < REG_COUNT; i++) {
      if (!((reg_list >> i) & 1)) {
        continue;
      }

      memcpy(mem, &r[i], sizeof(uint32_t));
      DEBUG_LOG("arm_stm: wrote r"
                << static_cast<int>(i) << "=" << std::hex << r[i] << " to addr="
                << std::hex << reinterpret_cast<uintptr_t>(mem) << std::dec);
      mem += 4;
    }
  }

  if (write_back) {
    r[rn] = add ? base + n * 4 : base - n * 4;
    DEBUG_LOG("arm_stm: write back to r" << static_cast<int>(rn) << "="
                                         << std::hex << r[rn] << std::dec);
  }
}

[[gnu::always_inline]]
inline void ProgramState::arm_ldrh(bool pre_indx, bool add, bool write_back,
                                   reg_idx_t rn, reg_idx_t rd, uint8_t type,
                                   uint32_t offset) {
  reg_value_t base = r[rn];
  reg_value_t addr = pre_indx ? base + (add ? offset : -offset) : base;

  DEBUG_LOG("arm_ldrh: r" << static_cast<int>(rn) << ", r"
                          << static_cast<int>(rd) << ", offset=" << offset
                          << ", type=" << static_cast<int>(type)
                          << ", addr=" << std::hex << addr << std::dec);

  const char *mem = reinterpret_cast<const char *>(address_resolve(addr));

  switch (type) {
  case 0b00:
    throw std::runtime_error("arm_ldrh: SWP unimplemented!");

  case 0b01:   // LDRH
    r[rd] = 0; // Clear upper bits
    memcpy(&r[rd], mem, sizeof(uint16_t));
    DEBUG_LOG("arm_ldrh: read halfword value=" << std::hex << r[rd]
                                               << " from addr=" << std::hex
                                               << addr << std::dec);
    break;

  case 0b10: // LDRSB
    int8_t byte;
    memcpy(&byte, mem, sizeof(int8_t));
    r[rd] = static_cast<reg_value_t>(static_cast<int32_t>(byte));
    DEBUG_LOG("arm_ldrh: read signed byte value=" << std::hex << r[rd]
                                                  << " from addr=" << std::hex
                                                  << addr << std::dec);
    break;

  case 0b11: // LDRSH
    int16_t word;
    memcpy(&word, mem, sizeof(int16_t));
    r[rd] = static_cast<reg_value_t>(static_cast<int32_t>(word));
    DEBUG_LOG("arm_ldrh: read signed halfword value="
              << std::hex << r[rd] << " from addr=" << std::hex << addr
              << std::dec);
    break;
  }

  if (write_back) {
    r[rn] = pre_indx ? addr : base + (add ? offset : -offset);
    DEBUG_LOG("arm_ldrh: write back to r" << static_cast<int>(rn) << "="
                                          << std::hex << r[rn] << std::dec);
  }
}

[[gnu::always_inline]]
inline void ProgramState::arm_strh(bool pre_indx, bool add, bool write_back,
                                   reg_idx_t rn, reg_idx_t rd, uint8_t type,
                                   uint32_t offset) {
  reg_value_t base = r[rn];
  reg_value_t addr = pre_indx ? base + (add ? offset : -offset) : base;

  DEBUG_LOG("arm_strh: r" << static_cast<int>(rn) << ", r"
                          << static_cast<int>(rd) << ", offset=" << offset
                          << ", type=" << static_cast<int>(type)
                          << ", addr=" << std::hex << addr << std::dec);

  char *mem = reinterpret_cast<char *>(address_resolve(addr));

  switch (type) {
  case 0b00:
    throw std::runtime_error("arm_stmh: SWP unimplemented!");

  case 0b01: // STRH
    memcpy(mem, &r[rd], sizeof(uint16_t));
    DEBUG_LOG("arm_strh: wrote halfword value=" << std::hex << (r[rd] & 0xFFFF)
                                                << " to addr=" << std::hex
                                                << addr << std::dec);
    break;

  case 0b10: // STRSB - This seems like a typo, STR doesn't have signed byte.
             // It's likely a simple STRB.
    memcpy(mem, &r[rd], sizeof(int8_t));
    DEBUG_LOG("arm_strh: wrote byte value=" << std::hex << (r[rd] & 0xFF)
                                            << " to addr=" << std::hex << addr
                                            << std::dec);
    break;

  case 0b11: // STRSH - This seems like a typo, STR doesn't have signed
             // halfword. It's likely a simple STRH.
    memcpy(mem, &r[rd], sizeof(int16_t));
    DEBUG_LOG("arm_strh: wrote halfword value=" << std::hex << (r[rd] & 0xFFFF)
                                                << " to addr=" << std::hex
                                                << addr << std::dec);
    break;
  }

  if (write_back) {
    r[rn] = pre_indx ? addr : base + (add ? offset : -offset);
    DEBUG_LOG("arm_strh: write back to r" << static_cast<int>(rn) << "="
                                          << std::hex << r[rn] << std::dec);
  }
}
