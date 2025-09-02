#pragma once
#include <cstdint>
#include <cstring>
#include <mutex>

#ifndef LIBLAYER_STACK_BASE
#define LIBLAYER_STACK_BASE (0xC0000000) // Virtual address of stack pointer
#endif

#ifndef LIBLAYER_STACK_SIZE
#define LIBLAYER_STACK_SIZE (1024 * 1024) // Size of the stack (1 MB)
#endif

#ifndef LIBLAYER_MEMORY_BASE
#define LIBLAYER_MEMORY_BASE (0x10000000) // Virtual address of the memory
#endif

#ifndef LIBLAYER_MEMORY_SIZE
#define LIBLAYER_MEMORY_SIZE (1024 * 1024 * 16) // Size of the memory (16 MB)
#endif

#ifdef LIBLAYER_DEBUG
#define DEBUG_LOG(fmt, ...)                                                    \
  do {                                                                         \
    std::cout << fmt << std::endl;                                             \
  } while (0)
#else
#define DEBUG_LOG(fmt, ...)                                                    \
  do {                                                                         \
  } while (0)
#endif

typedef uint8_t reg_idx_t;
typedef uint32_t reg_value_t;

// REGISTERS
enum {
  REG_R0 = 0,
  REG_R1 = 1,
  REG_R2 = 2,
  REG_R3 = 3,
  REG_R4 = 4,
  REG_R5 = 5,
  REG_R6 = 6,
  REG_R7 = 7,
  REG_R8 = 8,
  REG_R9 = 9,
  REG_R10 = 10,
  REG_R11 = 11,
  REG_R12 = 12,
  REG_SP = 13,
  REG_LR = 14,
  REG_PC = 15,
  REG_COUNT = 16,
};

class ExecutionState {
private:
  std::mutex memory_mutex;

public:
  reg_value_t r[REG_COUNT] = {
      0, 0,
      0, 0,
      0, 0,
      0, 0,
      0, 0,
      0, 0,
      0, LIBLAYER_STACK_BASE + LIBLAYER_STACK_SIZE - 1, // stack ptr
      0, 0,
  };

  bool cs, /* carry set */
      vs;  /* overflow set */
  bool mi, /* negative */
      z;   /* zero */

  uint8_t stack[LIBLAYER_STACK_SIZE] = {0}; /* stack */
  uint8_t *memory = nullptr;                /* memory */

  inline ExecutionState() { memory_init(); }

  inline ~ExecutionState() { delete[] memory; }

  virtual uint32_t address_map(uintptr_t addr);
  virtual uintptr_t address_resolve(uint32_t addr);

  // Allocations

  void memory_init();
  void *memory_alloc(uint32_t size);
  void memory_free(void *p);

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
};

/* Conditions */
#include "conditions.hpp"

#ifdef LIBLAYER_IMPL
#include "armv4.cpp"  // ARMv4 (ARM instructions)
#include "memory.cpp" // addressing / alloc / free
#endif
