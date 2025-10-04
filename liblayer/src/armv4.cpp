#include "liblayer/debug.hpp"
#include "liblayer/execution_state.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <endian.h>
#include <stdexcept>

#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define BESWAP16(x) __builtin_bswap16(x)
#define BESWAP32(x) __builtin_bswap32(x)
#else
#define BESWAP16(x) (x)
#define BESWAP32(x) (x)
#endif

namespace layer {

void ExecutionState::arm_add(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

  if (s) {
    cs = __builtin_add_overflow(r[rn], op2_value, &r[rd]);
    int32_t unused;
    vs = __builtin_sadd_overflow(r[rn], op2_value, &unused);
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  } else {
    r[rd] = r[rn] + op2_value;
  }

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_adc(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

  reg_value_t operand = op2_value + cs;
  if (s) {
    cs = __builtin_add_overflow(r[rn], operand, &r[rd]);
    int32_t unused;
    vs = __builtin_sadd_overflow(r[rn], operand, &unused);
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  } else {
    r[rd] = r[rn] + operand;
  }

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_sub(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

  if (s) {
    cs = !__builtin_sub_overflow(r[rn], op2_value, &r[rd]);
    int32_t unused;
    vs = __builtin_ssub_overflow(r[rn], op2_value, &unused);
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  } else {
    r[rd] = r[rn] - op2_value;
  }

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_sbc(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

  reg_value_t operand = op2_value + !cs;
  if (s) {
    cs = !__builtin_sub_overflow(r[rn], operand, &r[rd]);
    int32_t unused;
    vs = __builtin_ssub_overflow(r[rn], operand, &unused);
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  } else {
    r[rd] = r[rn] - operand;
  }

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_cmp(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

  uint32_t result;
  cs = !__builtin_sub_overflow(r[rn], op2_value, &result);
  int32_t unused;
  vs = __builtin_ssub_overflow(r[rn], op2_value, &unused);
  mi = (result >> 31) & 1;
  z = !result;

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_mov(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

  r[rd] = op2_value;
  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_rsb(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

  if (s) {
    cs = !__builtin_sub_overflow(op2_value, r[rn], &r[rd]);
    int32_t unused;
    vs = __builtin_ssub_overflow(op2_value, r[rn], &unused);
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  } else {
    r[rd] = op2_value - r[rn];
  }

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_rsc(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

  reg_value_t operand = r[rn] + !cs;

  if (s) {
    cs = !__builtin_sub_overflow(op2_value, operand, &r[rd]);
    int32_t unused;
    vs = __builtin_ssub_overflow(op2_value, operand, &unused);
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
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
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_eor(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

  r[rd] = r[rn] ^ op2_value;

  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_orr(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

  r[rd] = r[rn] | op2_value;
  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_bic(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

  r[rd] = r[rn] & ~op2_value;
  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_mvn(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

  r[rd] = ~op2_value;

  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_tst(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

  reg_value_t result = r[rn] & op2_value;
  mi = (result >> 31) & 1;
  z = !result;

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_teq(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

  reg_value_t result = r[rn] ^ op2_value;
  mi = (result >> 31) & 1;
  z = !result;

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_cmn(bool s, Register rd, Register rn,
                             reg_value_t op2_value) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

  reg_value_t result;
  cs = __builtin_add_overflow(r[rn], op2_value, &result);
  int32_t unused;
  vs = __builtin_sadd_overflow(r[rn], op2_value, &unused);
  mi = (result >> 31) & 1;
  z = !result;

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_mul(bool s, Register rd, Register rn, Register rs,
                             Register rm) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

  if (UNLIKELY(rd == rm)) {
    LAYER_DBE_LOG(*this, "%s",
                  "Warning: UNPREDICTABLE: Rd and Rm must be different!");
  }

  r[rd] = r[rm] * r[rs];

  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_mla(bool s, Register rd, Register rn, Register rs,
                             Register rm) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

  if (UNLIKELY(rd == rm)) {
    LAYER_DBE_LOG(*this, "%s",
                  "Warning: UNPREDICTABLE: Rd and Rm must be different!");
  }

  r[rd] = r[rm] * r[rs] + r[rn];
  if (s) {
    mi = (r[rd] >> 31) & 1;
    z = !r[rd];
  }

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_mull(bool s, bool sign, Register rd_lo, Register rd_hi,
                              Register rm, Register rs) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

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

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_mlal(bool s, bool sign, Register rd_lo, Register rd_hi,
                              Register rm, Register rs) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

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

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_ldr(bool pre_indx, bool add, bool byte,
                             bool write_back, Register rn, Register rd,
                             reg_value_t offset) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

  reg_value_t base = r[rn];
  reg_value_t addr = pre_indx ? base + (add ? offset : -offset) : base;

  LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "virtual address: 0x%X",
                   addr);

  const void *mem = reinterpret_cast<const void *>(address_resolve(addr));

  LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "resolved to: %p", mem);

  if (UNLIKELY(!mem)) {
    LAYER_DBE_LOG(*this, "%s", "error: resolved address is 0x00000000!");
    LAYER_DBE_SEND_PAUSED(*this);

    throw std::runtime_error("arm_ldr: resolved address is 0x00000000");
  }

  if (byte) {
    memset(&r[rd], 0, sizeof(reg_value_t));
    memcpy(&r[rd], mem, sizeof(uint8_t));
  } else {
    memcpy(&r[rd], mem, sizeof(uint32_t));
  }

  // for big endian, both word and byte transfers need to be swapped
  r[rd] = BESWAP32(r[rd]);

  LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "value read: 0x%X", r[rd]);

  if (write_back || !pre_indx) {
    // SPECIAL CASE: write-back to PC is UNPREDICTABLE, catch that
    if (UNLIKELY(r[rn] == PC)) {
      LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "%s",
                       "UNPREDICTABLE: writeback to PC as Rn is not allowed!");
    }

    r[rn] = base + (add ? offset : -offset);
    LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT,
                     "wrote back to r%d: 0x%X", rn, r[rn]);
  }

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_str(bool pre_indx, bool add, bool byte,
                             bool write_back, Register rn, Register rd,
                             reg_value_t offset) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

  reg_value_t base = r[rn];
  reg_value_t value = r[rd];
  reg_value_t addr = pre_indx ? base + (add ? offset : -offset) : base;

  /* SPECIAL CASE: When RD is PC, it will actually store ADDR + 12. Since PC is
   * always ADDR + 8, we just add 4 to it. */
  if (rd == PC) {
    value += 4;
  }

  LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "virtual address: 0x%X",
                   addr);

  void *mem = reinterpret_cast<void *>(address_resolve(addr));

  if (UNLIKELY(!mem)) {
    LAYER_DBE_LOG(*this, "%s", "error: resolved address is 0x00000000!");
    LAYER_DBE_SEND_PAUSED(*this);

    throw std::runtime_error("arm_str: resolved address is 0x00000000");
  }

  LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "resolved to: %p", mem);

  // for big endian, we swap the value to make sure its stored as little-endian
  value = BESWAP32(value);

  if (byte) {
    memcpy(mem, &value, sizeof(uint8_t));
  } else {
    memcpy(mem, &value, sizeof(uint32_t));
  }

  LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "value wrote to %p: 0x%X",
                   mem, value);

  if (write_back || !pre_indx) {
    // SPECIAL CASE: write-back to PC is UNPREDICTABLE, catch that
    if (UNLIKELY(r[rn] == PC)) {
      LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "%s",
                       "UNPREDICTABLE: writeback to PC as Rn is not allowed!");
    }

    r[rn] = base + (add ? offset : -offset);
    LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT,
                     "wrote back to r%d: 0x%X", rn, r[rn]);
  }

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_ldrh(bool pre_indx, bool add, bool write_back,
                              Register rn, Register rd, uint8_t type,
                              uint32_t offset) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

  reg_value_t base = r[rn];
  reg_value_t addr = pre_indx ? base + (add ? offset : -offset) : base;

  LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "virtual address: 0x%X",
                   addr);

  const char *mem = reinterpret_cast<const char *>(address_resolve(addr));

  LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "resolved to: %p", mem);

  if (UNLIKELY(!mem)) {
    LAYER_DBE_LOG(*this, "%s", "error: resolved address is 0x00000000!");
    LAYER_DBE_SEND_PAUSED(*this);

    throw std::runtime_error("arm_ldrh: resolved address is 0x00000000");
  }

  switch (type) {
  case 0b00:
    LAYER_DBE_LOG(*this, "%s", "error: SWP is not implemented!");
    LAYER_DBE_SEND_PAUSED(*this);
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

  // for big endian, both word and byte transfers need to be swapped
  r[rd] = BESWAP32(r[rd]);

  LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "value read: 0x%X", r[rd]);

  if (write_back || !pre_indx) {
    // SPECIAL CASE: write-back to PC is UNPREDICTABLE, catch that
    if (UNLIKELY(r[rn] == PC)) {
      LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "%s",
                       "UNPREDICTABLE: writeback to PC as Rn is not allowed!");
    }

    r[rn] = base + (add ? offset : -offset);
    LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT,
                     "wrote back to r%d: 0x%X", rn, r[rn]);
  }

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_strh(bool pre_indx, bool add, bool write_back,
                              Register rn, Register rd, uint8_t type,
                              uint32_t offset) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

  reg_value_t base = r[rn];
  reg_value_t value = r[rd];
  reg_value_t addr = pre_indx ? base + (add ? offset : -offset) : base;

  /* SPECIAL CASE: When RD is PC, it will actually store ADDR + 12. Since PC
   * is always ADDR + 8, we just add 4 to it. */
  if (rd == PC) {
    value += 4;
  }

  LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "virtual address: 0x%X",
                   addr);

  char *mem = reinterpret_cast<char *>(address_resolve(addr));

  LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "resolved to: %p", mem);

  if (UNLIKELY(!mem)) {
    LAYER_DBE_LOG(*this, "%s", "error: resolved address is 0x00000000!");
    LAYER_DBE_SEND_PAUSED(*this);

    throw std::runtime_error("arm_strh: resolved address is 0x00000000");
  }

  // for big endian, we swap the value to make sure its stored as little-endian
  value = BESWAP32(value);

  switch (type) {
  case 0b00:
    LAYER_DBE_LOG(*this, "%s", "error: SWP is not implemented!");
    LAYER_DBE_SEND_PAUSED(*this);
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

  LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "value stored to %p: 0x%X",
                   mem, r[rd]);

  if (write_back || !pre_indx) {
    // SPECIAL CASE: write-back to PC is UNPREDICTABLE, catch that
    if (UNLIKELY(r[rn] == PC)) {
      LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "%s",
                       "UNPREDICTABLE: writeback to PC as Rn is not allowed!");
    }

    r[rn] = base + (add ? offset : -offset);
    LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT,
                     "wrote back to r%d: 0x%X", rn, r[rn]);
  }

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_ldm(bool pre_indx, bool add, bool write_back,
                             Register rn, reg_value_t reg_list) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

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
    LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT,
                     "wrote back to r%d: 0x%X", rn, r[rn]);
  }

  LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "virtual address: 0x%X",
                   addr);

  const char *mem = reinterpret_cast<const char *>(address_resolve(addr));

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
    r[i] = BESWAP32(r[i]);

    LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT,
                     "value read from %p: 0x%X", mem, r[i]);

    LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "%s: after write",
                     __func__);
    mem += 4;
  }

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

void ExecutionState::arm_stm(bool pre_indx, bool add, bool write_back,
                             Register rn, reg_value_t reg_list) {
  LAYER_DBE_NEXT(*this, "%s: before", __func__);

  reg_value_t base = r[rn];
  reg_value_t n = __builtin_popcount(reg_list);
  reg_value_t addr;

  if (add) {
    addr = pre_indx ? base + 4 : base;
  } else {
    addr = pre_indx ? base - (n * 4) : base - 4;
  }

  LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT, "virtual address: 0x%X",
                   addr);

  char *mem = reinterpret_cast<char *>(address_resolve(addr));
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

    if (!write_back || written) {
      continue;
    }

    // We write-back now
    r[rn] = add ? base + n * 4 : base - n * 4;
    written = true;

    LAYER_DBE_LOG_IF(*this, dbe.flags & Debugee::NEXT,
                     "wrote back to r%d: 0x%X", rn, r[rn]);
  }

  LAYER_DBE_NEXT(*this, "%s: after", __func__);
}

} // namespace layer
