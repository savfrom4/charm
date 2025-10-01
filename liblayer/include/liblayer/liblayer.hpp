#pragma once
#include "conditions.hpp" // for condition code macros
#include "debug.hpp"      // for debugging support

#include <cstdint>
#include <mutex>
#include <string>
#include <type_traits>

// -------------------------------------
// ------------- OPTIONS ---------------
// -------------------------------------

#ifndef LAYER_STACK_BASE
#define LAYER_STACK_BASE (0xC0000000) // Virtual address of stack pointer
#endif

#ifndef LAYER_MEMORY_BASE
#define LAYER_MEMORY_BASE (0x10000000) // Virtual address of the memory
#endif

#ifndef LAYER_STACK_SIZE
#define LAYER_STACK_SIZE (1024 * 1024 * 4) // Size of the stack (4 MB)
#endif

#ifndef LAYER_MEMORY_SIZE
#define LAYER_MEMORY_SIZE (1024 * 1024 * 16) // Size of the memory (16 MB)
#endif

namespace layer {

typedef uint8_t reg_idx_t;
typedef uint32_t reg_value_t;

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
      0, 0,
      0, 0,
      0, 0,
      0, 0,
      0, 0,
      0, 0,
      0, LAYER_STACK_BASE + LAYER_STACK_SIZE - 1, // stack pointer
      0, 0,
  };

  uint8_t stack[LAYER_STACK_SIZE] = {0};            /* stack */
  uint8_t *memory = new uint8_t[LAYER_MEMORY_SIZE]; /* memory */

  inline ExecutionState() { memory_init(); }
  inline ~ExecutionState() { delete[] memory; }

  // Memory

  void memory_init();
  void *memory_alloc(uint32_t size);
  void memory_free(void *p);

  // Addressing

  template <typename T> inline uint32_t address_map(T address) {
    static_assert(std::is_pointer_v<T>, "T must be a pointer!");
    return address_map(reinterpret_cast<uintptr_t>(address));
  }

  template <typename T> inline T address_resolve(uint32_t address) {
    static_assert(std::is_pointer_v<T>, "T must be a pointer!");
    return reinterpret_cast<T>(address_resolve(address));
  }

  virtual uint32_t address_map(uintptr_t address);
  virtual uintptr_t address_resolve(uint32_t address);

  // Dumping/restoring state from a file
  void dump(const std::string &filename);
  void restore(const std::string &filename);

  // armv4.cpp

  void arm_add(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t op2_value);
  void arm_adc(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t op2_value);
  void arm_sub(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t op2_value);
  void arm_sbc(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t op2_value);
  void arm_cmp(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t op2_value);
  void arm_mov(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t op2_value);
  void arm_rsb(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t op2_value);
  void arm_rsc(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t op2_value);
  void arm_and(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t op2_value);
  void arm_eor(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t op2_value);
  void arm_orr(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t op2_value);
  void arm_bic(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t op2_value);
  void arm_mvn(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t op2_value);
  void arm_tst(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t op2_value);
  void arm_teq(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t op2_value);
  void arm_cmn(bool s, reg_idx_t rd, reg_idx_t rn, reg_value_t op2_value);

  void arm_mul(bool s, reg_idx_t rd, reg_idx_t rn, reg_idx_t rs, reg_idx_t rm);
  void arm_mla(bool s, reg_idx_t rd, reg_idx_t rn, reg_idx_t rs, reg_idx_t rm);

  void arm_mull(bool s, bool sign, reg_idx_t rd_hi, reg_idx_t rd_lo,
                reg_idx_t rs, reg_idx_t rm);
  void arm_mlal(bool s, bool sign, reg_idx_t rd_hi, reg_idx_t rd_lo,
                reg_idx_t rs, reg_idx_t rm);

  void arm_ldr(bool pre_indx, bool add, bool byte, bool write_back,
               reg_idx_t rn, reg_idx_t rd, reg_value_t offset);
  void arm_str(bool pre_indx, bool add, bool byte, bool write_back,
               reg_idx_t rn, reg_idx_t rd, reg_value_t offset);
  void arm_ldm(bool pre_indx, bool add, bool write_back, reg_idx_t rn,
               reg_value_t reg_list);
  void arm_stm(bool pre_indx, bool add, bool write_back, reg_idx_t rn,
               reg_value_t reg_list);
  void arm_ldrh(bool pre_indx, bool add, bool write_back, reg_idx_t rn,
                reg_idx_t rd, uint8_t type, reg_value_t offset);
  void arm_strh(bool pre_indx, bool add, bool write_back, reg_idx_t rn,
                reg_idx_t rd, uint8_t type, reg_value_t offset);

  // TODO: implement armv5, add thumbv1

private:
// connection to the debugger is only present when LAYER_DEBUG
#ifdef LAYER_DEBUG
  ExecutionDebugee _dbg;
#endif

  std::mutex _memory_lock;
};

} // namespace layer
