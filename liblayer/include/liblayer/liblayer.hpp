#pragma once
#include <cstdint>
#include <mutex>
#include <type_traits>

#ifndef LAYER_STACK_BASE
#define LAYER_STACK_BASE (0xC0000000) // Virtual address of stack pointer
#endif

#ifndef LAYER_STACK_SIZE
#define LAYER_STACK_SIZE (1024 * 1024 * 4) // Size of the stack (4 MB)
#endif

#ifndef LAYER_MEMORY_BASE
#define LAYER_MEMORY_BASE (0x10000000) // Virtual address of the memory
#endif

#ifndef LAYER_MEMORY_SIZE
#define LAYER_MEMORY_SIZE (1024 * 1024 * 4) // Size of the memory (16 MB)
#endif

#ifdef LAYER_DEBUG
#include <iostream>
#define LAYER_LOG(fmt, ...)                                                    \
  do {                                                                         \
    std::cout << fmt << std::endl;                                             \
  } while (0)
#else
#define LAYER_LOG(fmt, ...)                                                    \
  do {                                                                         \
  } while (0)
#endif

/* Conditions */
#include "conditions.hpp"

namespace layer {

typedef uint8_t reg_idx_t;
typedef uint32_t reg_value_t;

// REGISTERS
enum : reg_idx_t {
  R0 = 0,
  R1 = 1,
  R2 = 2,
  R3 = 3,
  R4 = 4,
  R5 = 5,
  R6 = 6,
  R7 = 7,
  R8 = 8,
  R9 = 9,
  R10 = 10,
  R11 = 11,
  R12 = 12,
  SP = 13,
  LR = 14,
  PC = 15,
  REG_COUNT = 16,
};

class ExecutionState {
public:
  bool cs, /* carry set */
      vs;  /* overflow set */
  bool mi, /* negative */
      z;   /* zero */

  reg_value_t r[REG_COUNT] = {
      0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, LAYER_STACK_BASE + LAYER_STACK_SIZE - 1, // stack ptr
      0, 0,
  };

  uint8_t stack[LAYER_STACK_SIZE] = {0}; /* stack */
  uint8_t *memory = nullptr;             /* memory */

  inline ExecutionState() {
    memory = new uint8_t[LAYER_MEMORY_SIZE];
    memory_init();
  }

  inline ~ExecutionState() { delete[] memory; }

  // Memory

  void memory_init();
  void *memory_alloc(uint32_t size);
  void memory_free(void *p);

  virtual uint32_t memory_map(uintptr_t address);
  virtual uintptr_t memory_resolve(uint32_t address);

  template <typename T> inline uint32_t memory_map(T address) {
    static_assert(std::is_pointer_v<T>, "T must be a pointer!");
    return memory_map(reinterpret_cast<uintptr_t>(address));
  }

  template <typename T> inline T memory_resolve(uint32_t address) {
    static_assert(std::is_pointer_v<T>, "T must be a pointer!");
    return reinterpret_cast<T>(memory_resolve(address));
  }

  // armv4

  void arm_add(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t imm);
  void arm_adc(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t imm);
  void arm_sub(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t imm);
  void arm_sbc(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t imm);
  void arm_cmp(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t imm);
  void arm_mov(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t imm);
  void arm_rsb(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t imm);
  void arm_rsc(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t imm);
  void arm_and(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t imm);
  void arm_eor(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t imm);
  void arm_orr(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t imm);
  void arm_bic(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t imm);
  void arm_mvn(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t imm);
  void arm_tst(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t imm);
  void arm_teq(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t imm);
  void arm_cmn(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t imm);

  void arm_mul(bool s, reg_idx_t rd, reg_idx_t rn, reg_idx_t rs, reg_idx_t rm);
  void arm_mla(bool s, reg_idx_t rd, reg_idx_t rn, reg_idx_t rs, reg_idx_t rm);

  void arm_mull(bool s, bool sign, reg_idx_t rd_hi, reg_idx_t rd_lo,
                reg_idx_t rs, reg_idx_t rm);
  void arm_mlal(bool s, bool sign, reg_idx_t rd_hi, reg_idx_t rd_lo,
                reg_idx_t rs, reg_idx_t rm);

  void arm_ldr(bool pre_indx, bool add, bool byte, bool write_back,
               reg_idx_t rn, reg_idx_t rd, reg_value_t offset, bool copy);
  void arm_str(bool pre_indx, bool add, bool byte, bool write_back,
               reg_idx_t rn, reg_idx_t rd, reg_value_t offset, bool copy);
  void arm_ldm(bool pre_indx, bool add, bool write_back, reg_idx_t rn,
               reg_value_t reg_list, bool copy);
  void arm_stm(bool pre_indx, bool add, bool write_back, reg_idx_t rn,
               reg_value_t reg_list, bool copy);
  void arm_ldrh(bool pre_indx, bool add, bool write_back, reg_idx_t rn,
                reg_idx_t rd, uint8_t type, uint32_t offset);
  void arm_strh(bool pre_indx, bool add, bool write_back, reg_idx_t rn,
                reg_idx_t rd, uint8_t type, uint32_t offset);

  /* THUMB instructions */
  // TODO: add thumb

private:
  std::mutex memory_mutex;
};

} // namespace layer

#ifdef LAYER_IMPLEMENTATION
#include "armv4.cpp"  // ARMv4 (ARM instructions)
#include "memory.cpp" // addressing / alloc / free
#endif
