#include "liblayer.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <stdexcept>

#define UNLIKELY(x) __builtin_expect(!!(x), 0)

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

inline void ProgramState::arm_add(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_add: before r" << static_cast<int>(rd) << "=" << r[rd] << ", r"
                                << static_cast<int>(rn) << "=" << r[rn]
                                << ", imm=" << imm);

  if (s) {
    cs = __builtin_add_overflow(r[rn], imm, &r[rd]);
    int32_t unused;
    vs = __builtin_sadd_overflow(r[rn], imm, &unused);
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  } else {
    r[rd] = r[rn] + imm;
  }

  DEBUG_LOG("arm_add: after r" << static_cast<int>(rd) << "=" << r[rd]);
}

inline void ProgramState::arm_adc(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_adc: before r" << static_cast<int>(rd) << "=" << r[rd] << ", r"
                                << static_cast<int>(rn) << "=" << r[rn]
                                << ", imm=" << imm << ", cs=" << cs);

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

  DEBUG_LOG("arm_adc: after r" << static_cast<int>(rd) << "=" << r[rd]);
}

inline void ProgramState::arm_sub(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_sub: before r" << static_cast<int>(rd) << "=" << r[rd] << ", r"
                                << static_cast<int>(rn) << "=" << r[rn]
                                << ", imm=" << imm);

  if (s) {
    cs = !__builtin_sub_overflow(r[rn], imm, &r[rd]);
    int32_t unused;
    vs = __builtin_ssub_overflow(r[rn], imm, &unused);
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  } else {
    r[rd] = r[rn] - imm;
  }

  DEBUG_LOG("arm_sub: after r" << static_cast<int>(rd) << "=" << r[rd]);
}

inline void ProgramState::arm_sbc(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_sbc: before r" << static_cast<int>(rd) << "=" << r[rd] << ", r"
                                << static_cast<int>(rn) << "=" << r[rn]
                                << ", imm=" << imm << ", cs=" << cs);

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

  DEBUG_LOG("arm_sbc: after r" << static_cast<int>(rd) << "=" << r[rd]);
}

inline void ProgramState::arm_cmp(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_cmp: before r" << static_cast<int>(rn) << "=" << r[rn]
                                << ", imm=" << imm);

  uint32_t result;
  cs = !__builtin_sub_overflow(r[rn], imm, &result);
  int32_t unused;
  vs = __builtin_ssub_overflow(r[rn], imm, &unused);
  mi = (result >> 31) & 1;
  z = !result;

  DEBUG_LOG("arm_cmp: result=" << result << " (flags: N=" << mi << ", Z=" << z
                               << ", C=" << cs << ", V=" << vs << ")");
}

inline void ProgramState::arm_mov(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_mov: before r" << static_cast<int>(rd) << "=" << r[rd]
                                << ", imm=" << imm);

  r[rd] = imm;
  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  DEBUG_LOG("arm_mov: after r" << static_cast<int>(rd) << "=" << r[rd]);
}

inline void ProgramState::arm_rsb(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_rsb: before r" << static_cast<int>(rd) << "=" << r[rd] << ", r"
                                << static_cast<int>(rn) << "=" << r[rn]
                                << ", imm=" << imm);

  if (s) {
    cs = !__builtin_sub_overflow(imm, r[rn], &r[rd]);
    int32_t unused;
    vs = __builtin_ssub_overflow(imm, r[rn], &unused);
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  } else {
    r[rd] = imm - r[rn];
  }

  DEBUG_LOG("arm_rsb: after r" << static_cast<int>(rd) << "=" << r[rd]);
}

inline void ProgramState::arm_rsc(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_rsc: before r" << static_cast<int>(rd) << "=" << r[rd] << ", r"
                                << static_cast<int>(rn) << "=" << r[rn]
                                << ", imm=" << imm << ", cs=" << cs);

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

  DEBUG_LOG("arm_rsc: after r" << static_cast<int>(rd) << "=" << r[rd]);
}

inline void ProgramState::arm_and(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_and: before r" << static_cast<int>(rd) << "=" << r[rd] << ", r"
                                << static_cast<int>(rn) << "=" << r[rn]
                                << ", imm=" << imm);
  r[rd] = r[rn] & imm;

  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  DEBUG_LOG("arm_and: after r" << static_cast<int>(rd) << "=" << r[rd]);
}

inline void ProgramState::arm_eor(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_eor: before r" << static_cast<int>(rd) << "=" << r[rd] << ", r"
                                << static_cast<int>(rn) << "=" << r[rn]
                                << ", imm=" << imm);
  r[rd] = r[rn] ^ imm;

  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  DEBUG_LOG("arm_eor: after r" << static_cast<int>(rd) << "=" << r[rd]);
}

inline void ProgramState::arm_orr(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_orr: before r" << static_cast<int>(rd) << "=" << r[rd] << ", r"
                                << static_cast<int>(rn) << "=" << r[rn]
                                << ", imm=" << imm);
  r[rd] = r[rn] | imm;
  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  DEBUG_LOG("arm_orr: after r" << static_cast<int>(rd) << "=" << r[rd]);
}

inline void ProgramState::arm_bic(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_bic: before r" << static_cast<int>(rd) << "=" << r[rd] << ", r"
                                << static_cast<int>(rn) << "=" << r[rn]
                                << ", imm=" << imm);
  r[rd] = r[rn] & ~imm;

  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  DEBUG_LOG("arm_bic: after r" << static_cast<int>(rd) << "=" << r[rd]);
}

inline void ProgramState::arm_mvn(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_mvn: before r" << static_cast<int>(rd) << "=" << r[rd]
                                << ", imm=" << imm);
  r[rd] = ~imm;

  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  DEBUG_LOG("arm_mvn: after r" << static_cast<int>(rd) << "=" << r[rd]);
}

inline void ProgramState::arm_tst(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_tst: before r" << static_cast<int>(rn) << "=" << r[rn]
                                << ", imm=" << imm);

  reg_value_t result = r[rn] & imm;
  mi = (result >> 31) & 1;
  z = !result;

  DEBUG_LOG("arm_tst: result=" << result << " (flags: N=" << mi << ", Z=" << z
                               << ")");
}

inline void ProgramState::arm_teq(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_teq: before r" << static_cast<int>(rn) << "=" << r[rn]
                                << ", imm=" << imm);

  reg_value_t result = r[rn] ^ imm;
  mi = (result >> 31) & 1;
  z = !result;

  DEBUG_LOG("arm_teq: result=" << result << " (flags: N=" << mi << ", Z=" << z
                               << ")");
}

inline void ProgramState::arm_cmn(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_value_t imm) {
  DEBUG_LOG("arm_cmn: before r" << static_cast<int>(rn) << "=" << r[rn]
                                << ", imm=" << imm);
  reg_value_t result;
  cs = __builtin_add_overflow(r[rn], imm, &result);
  int32_t unused;
  vs = __builtin_sadd_overflow(r[rn], imm, &unused);
  mi = (result >> 31) & 1;
  z = !result;

  DEBUG_LOG("arm_cmn: result=" << result << " (flags: N=" << mi << ", Z=" << z
                               << ", C=" << cs << ", V=" << vs << ")");
}

inline void ProgramState::arm_mul(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_idx_t rs, reg_idx_t rm) {
  DEBUG_LOG("arm_mul: before r" << static_cast<int>(rd) << "=" << r[rd] << ", r"
                                << static_cast<int>(rm) << "=" << r[rm] << ", r"
                                << static_cast<int>(rs) << "=" << r[rs]);
  r[rd] = r[rm] * r[rs];

  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  DEBUG_LOG("arm_mul: after r" << static_cast<int>(rd) << "=" < < < r[rd]);
}

inline void ProgramState::arm_mla(bool s, reg_idx_t rd, reg_idx_t rn,
                                  reg_idx_t rs, reg_idx_t rm) {
  DEBUG_LOG("arm_mla: before r" << static_cast<int>(rd) << "=" << r[rd] << ", r"
                                << static_cast<int>(rm) << "=" << r[rm] << ", r"
                                << static_cast<int>(rs) << "=" << r[rs] << ", r"
                                << static_cast<int>(rn) << "=" << r[rn]);
  r[rd] = r[rm] * r[rs] + r[rn];

  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  DEBUG_LOG("arm_mla: after r" << static_cast<int>(rd) << "=" << r[rd]);
}

inline void ProgramState::arm_mull(bool s, bool sign, reg_idx_t rd_lo,
                                   reg_idx_t rd_hi, reg_idx_t rm,
                                   reg_idx_t rs) {
  DEBUG_LOG("arm_mull: before r" << static_cast<int>(rd_lo) << "=" << r[rd_lo]
                                 << ", r" << static_cast<int>(rd_hi) << "="
                                 << r[rd_hi] << ", r" << static_cast<int>(rm)
                                 << "=" << r[rm] << ", r"
                                 << static_cast<int>(rs) << "=" << r[rs]);
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

  DEBUG_LOG("arm_mull: result=" << result << " (r" << static_cast<int>(rd_lo)
                                << "=" << std::hex << r[rd_lo] << ", r"
                                << static_cast<int>(rd_hi) << "=" << r[rd_hi]
                                << ")");

  if (s) {
    mi = (result >> 63) & 1;
    z = (result == 0);
  }
}

inline void ProgramState::arm_mlal(bool s, bool sign, reg_idx_t rd_lo,
                                   reg_idx_t rd_hi, reg_idx_t rm,
                                   reg_idx_t rs) {
  DEBUG_LOG("arm_mlal: before r" << static_cast<int>(rd_lo) << "=" << r[rd_lo]
                                 << ", r" << static_cast<int>(rd_hi) << "="
                                 << r[rd_hi] << ", r" << static_cast<int>(rm)
                                 << "=" << r[rm] << ", r"
                                 << static_cast<int>(rs) << "=" << r[rs]);

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
            << acc << " (r" << static_cast<int>(rd_lo) << "=" << r[rd_lo]
            << ", r" << static_cast<int>(rd_hi) << "=" << r[rd_hi] << ")");

  if (s) {
    mi = (acc >> 63) & 1;
    z = (acc == 0);
  }
}

inline void ProgramState::arm_ldr(bool pre_indx, bool add, bool byte,
                                  bool write_back, reg_idx_t rn, reg_idx_t rd,
                                  reg_value_t offset, bool copy) {
  reg_value_t base = r[rn];
  reg_value_t addr = pre_indx ? base + (add ? offset : -offset) : base;

  DEBUG_LOG("arm_ldr: r" << static_cast<int>(rd) << ", r"
                         << static_cast<int>(rn) << ", offset=0x" << offset
                         << ", pre_indx=0x" << pre_indx << ", add=0x" << add
                         << ", byte=0x" << byte << ", write_back=0x"
                         << write_back << ", addr=0x" << std::hex << addr
                         << std::dec);

  if (copy) {
    const void *mem = reinterpret_cast<const void *>(address_resolve(addr));

    if (byte) {
      memset(&r[rd], 0, sizeof(reg_value_t));
      memcpy(&r[rd], mem, sizeof(uint8_t));
    } else {
      memcpy(&r[rd], mem, sizeof(uint32_t));
    }
    DEBUG_LOG("arm_ldr: read value=0x" << std::hex << r[rd] << std::dec);
  }

  if (write_back || !pre_indx) {
    // SPECIAL CASE: write-back to PC is UNPREDICTABLE, catch that
    if (UNLIKELY(r[rn] == REG_PC)) {
      DEBUG_LOG(
          "arm_ldr: UNPREDICTABLE: Write-back to PC as Rn is not allowed!");
    }

    r[rn] = base + (add ? offset : -offset);
    DEBUG_LOG("arm_ldr: write back to r" << static_cast<int>(rn) << "=0x"
                                         << std::hex << r[rn] << std::dec);
  }
}

inline void ProgramState::arm_str(bool pre_indx, bool add, bool byte,
                                  bool write_back, reg_idx_t rn, reg_idx_t rd,
                                  reg_value_t offset, bool copy) {
  reg_value_t base = r[rn];
  reg_value_t value = r[rd];
  reg_value_t addr = pre_indx ? base + (add ? offset : -offset) : base;

  /* SPECIAL CASE: When RD is PC, it will actually store ADDR + 12. Since PC is
   * always ADDR + 8, we just add 4 to it. */
  if (rd == REG_PC) {
    value += 4;
  }

  DEBUG_LOG("arm_str: r" << static_cast<int>(rd) << ", r"
                         << static_cast<int>(rn) << ", offset=0x" << offset
                         << ", pre_indx=0x" << pre_indx << ", add=0x" << add
                         << ", byte=0x" << byte << ", write_back=0x"
                         << write_back << ", addr=0x" << std::hex << addr
                         << std::dec);

  if (copy) {
    void *mem = reinterpret_cast<void *>(address_resolve(addr));

    if (byte) {
      memcpy(mem, &value, sizeof(uint8_t));
      DEBUG_LOG("arm_str: wrote value=0x"
                << std::hex << (static_cast<uint8_t>(value)) << ", addr=0x"
                << addr << std::dec);
    } else {
      memcpy(mem, &value, sizeof(uint32_t));
      DEBUG_LOG("arm_str: wrote value=0x" << std::hex << value << ", addr=0x"
                                          << addr << std::dec);
    }
  }

  if (write_back || !pre_indx) {
    // SPECIAL CASE: write-back to PC is UNPREDICTABLE, catch that
    if (UNLIKELY(r[rn] == REG_PC)) {
      DEBUG_LOG(
          "arm_str: UNPREDICTABLE: Write-back to PC as Rn is not allowed!");
    }

    r[rn] = base + (add ? offset : -offset);
    DEBUG_LOG("arm_str: write back to r" << static_cast<int>(rn) << "=0x"
                                         << std::hex << r[rn] << std::dec);
  }
}

inline void ProgramState::arm_ldrh(bool pre_indx, bool add, bool write_back,
                                   reg_idx_t rn, reg_idx_t rd, uint8_t type,
                                   uint32_t offset) {
  reg_value_t base = r[rn];
  reg_value_t addr = pre_indx ? base + (add ? offset : -offset) : base;

  DEBUG_LOG("arm_ldrh: r" << static_cast<int>(rn) << ", r"
                          << static_cast<int>(rd) << ", offset=0x" << offset
                          << ", type=0x" << static_cast<int>(type)
                          << ", addr=0x" << std::hex << addr << std::dec);

  const char *mem = reinterpret_cast<const char *>(address_resolve(addr));

  switch (type) {
  case 0b00:
    throw std::runtime_error("arm_ldrh: SWP unimplemented!");

  case 0b01: // LDRHR
    r[rd] = 0;
    memcpy(&r[rd], mem, sizeof(uint16_t));

    DEBUG_LOG("arm_ldrh: read halfword value=0x"
              << std::hex << r[rd] << ", addr=0x" << std::hex << addr
              << std::dec);
    break;

  case 0b10: // LDRSBR
    int8_t byte;
    memcpy(&byte, mem, sizeof(int8_t));
    r[rd] = static_cast<reg_value_t>(static_cast<int32_t>(byte));

    DEBUG_LOG("arm_ldrh: read signed byte value=0x"
              << std::hex << static_cast<int32_t>(byte) << ", addr=0x"
              << std::hex << addr << std::dec);
    break;

  case 0b11: // LDRSHR
    int16_t word;
    memcpy(&word, mem, sizeof(int16_t));
    r[rd] = static_cast<reg_value_t>(static_cast<int32_t>(word));

    DEBUG_LOG("arm_ldrh: read signed halfword value=0x"
              << std::hex << static_cast<int32_t>(word) << ", addr=0x"
              << std::hex << addr << std::dec);
    break;
  }

  if (write_back || !pre_indx) {
    // SPECIAL CASE: write-back to PC is UNPREDICTABLE, catch that
    if (UNLIKELY(r[rn] == REG_PC)) {
      DEBUG_LOG(
          "arm_ldrh: UNPREDICTABLE: Write-back to PC as Rn is not allowed!");
    }

    r[rn] = base + (add ? offset : -offset);
    DEBUG_LOG("arm_ldrh: write back to r" << static_cast<int>(rn) << "=0x"
                                          << std::hex << r[rn] << std::dec);
  }
}

inline void ProgramState::arm_strh(bool pre_indx, bool add, bool write_back,
                                   reg_idx_t rn, reg_idx_t rd, uint8_t type,
                                   uint32_t offset) {
  reg_value_t base = r[rn];
  reg_value_t value = r[rd];
  reg_value_t addr = pre_indx ? base + (add ? offset : -offset) : base;

  /* SPECIAL CASE: When RD is PC, it will actually store ADDR + 12. Since PC is
   * always ADDR + 8, we just add 4 to it. */
  if (rd == REG_PC) {
    value += 4;
  }

  DEBUG_LOG("arm_strh: r" << static_cast<int>(rn) << ", r"
                          << static_cast<int>(rd) << ", offset=0x" << offset
                          << ", type=0x" << static_cast<int>(type)
                          << ", addr=0x" << std::hex << addr << std::dec);

  char *mem = reinterpret_cast<char *>(address_resolve(addr));

  switch (type) {
  case 0b00:
    throw std::runtime_error("arm_stmh: SWP unimplemented!");

  case 0b01: // STRHR
    memcpy(mem, &value, sizeof(uint16_t));
    DEBUG_LOG("arm_strh: wrote halfword value=0x"
              << std::hex << static_cast<uint16_t>(value) << ", addr=0x"
              << std::hex << addr << std::dec);
    break;

  case 0b10: // STRSBR
    memcpy(mem, &value, sizeof(int8_t));
    DEBUG_LOG("arm_strh: wrote byte value=0x"
              << std::hex << static_cast<int8_t>(value) << ", addr=0x"
              << std::hex << addr << std::dec);
    break;

  case 0b11: // STRSHR
    memcpy(mem, &value, sizeof(int16_t));
    DEBUG_LOG("arm_strh: wrote halfword value=0x"
              << std::hex << static_cast<int16_t>(value) << ", addr=0x"
              << std::hex << addr << std::dec);
    break;
  }

  if (write_back || !pre_indx) {
    // SPECIAL CASE: write-back to PC is UNPREDICTABLE, catch that
    if (UNLIKELY(r[rn] == REG_PC)) {
      DEBUG_LOG(
          "arm_strh: UNPREDICTABLE: Write-back to PC as Rn is not allowed!");
    }

    r[rn] = base + (add ? offset : -offset);
    DEBUG_LOG("arm_strh: write back to r" << static_cast<int>(rn) << "=0x"
                                          << std::hex << r[rn] << std::dec);
  }
}

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

  DEBUG_LOG("arm_ldm: r" << static_cast<int>(rn) << ", reg_list=0x" << std::hex
                         << reg_list << ", pre_indx=0x" << pre_indx
                         << ", add=0x" << add << ", write_back=0x" << write_back
                         << ", base=0x" << base << ", starting addr=0x" << addr
                         << std::dec);

  if (write_back) {
    r[rn] = add ? base + n * 4 : base - n * 4;
    DEBUG_LOG("arm_ldm: write back to r" << static_cast<int>(rn) << "=0x"
                                         << std::hex << r[rn] << std::dec);
  }

  if (copy) {
    const char *mem = reinterpret_cast<const char *>(address_resolve(addr));
    for (reg_value_t i = 0; i < REG_COUNT; i++) {
      if (!((reg_list >> i) & 1)) {
        continue;
      }

      memcpy(&r[i], mem, sizeof(uint32_t));
      DEBUG_LOG("arm_ldm: read r" << static_cast<int>(i) << "=0x" << std::hex
                                  << r[i] << ", addr=0x" << std::hex
                                  << reinterpret_cast<uintptr_t>(mem)
                                  << std::dec);
      mem += 4;
    }
  }
}

inline void ProgramState::arm_stm(bool pre_indx, bool add, bool write_back,
                                  reg_idx_t rn, reg_value_t reg_list,
                                  bool copy) {
  reg_value_t base = r[rn];
  reg_value_t n = __builtin_popcount(reg_list);
  reg_value_t addr;

  if (add) {
    addr = pre_indx ? base + 4 : base;
  } else {
    addr = pre_indx ? base - n * 4 : base - 4;
  }

  DEBUG_LOG("arm_stm: r" << static_cast<int>(rn) << ", reg_list=0x" << std::hex
                         << reg_list << ", pre_indx=0x" << pre_indx
                         << ", add=0x" << add << ", write_back=0x" << write_back
                         << ", base=0x" << base << ", starting addr=0x" << addr
                         << std::dec);

  if (copy) {
    char *mem = reinterpret_cast<char *>(address_resolve(addr));
    bool written = false;

    for (reg_value_t i = 0; i < REG_COUNT; i++) {
      if (!((reg_list >> i) & 1)) {
        continue;
      }

      memcpy(mem, &r[i], sizeof(uint32_t));
      DEBUG_LOG("arm_stm: wrote r" << static_cast<int>(i) << "=0x" << std::hex
                                   << r[i] << ", addr=0x" << std::hex
                                   << reinterpret_cast<uintptr_t>(mem)
                                   << std::dec);
      mem += 4;

      if (!write_back || written) {
        continue;
      }

      // We write-back now
      r[rn] = add ? base + n * 4 : base - n * 4;
      DEBUG_LOG("arm_stm: write back (n = " << n << ") to r"
                                            << static_cast<int>(rn) << "=0x"
                                            << std::hex << r[rn] << std::dec);
      written = true;
    }
  }
}
