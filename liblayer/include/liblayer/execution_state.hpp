#pragma once
#include <array>
#include <cstdint>
#include <mutex>
#include <vector>

// -------------------------------------
// ------------- OPTIONS ---------------
// -------------------------------------

#ifndef LAYER_STACK_BASE
#define LAYER_STACK_BASE                                                       \
  (0xC0000000) // Virtual address of stack pointer (must be word-aligned)
#endif

#ifndef LAYER_MEMORY_BASE
#define LAYER_MEMORY_BASE                                                      \
  (0x10000000) // Virtual address of the memory (must be word-aligned)
#endif

#ifndef LAYER_STACK_SIZE
#define LAYER_STACK_SIZE                                                       \
  (1024 * 1024 * 4) // Size of the stack (4 MiB, must be word-aligned)
#endif

#ifndef LAYER_MEMORY_SIZE
#define LAYER_MEMORY_SIZE                                                      \
  (1024 * 1024 * 16) // Size of the memory (16 MiB, must be world-aligned)
#endif

#ifndef LAYER_MEMORY_BLOCK_SIZE
#define LAYER_MEMORY_BLOCK_SIZE (64) // Min allocation (must be word-aligned)
#endif

namespace layer {

typedef std::uint32_t reg_value_t;

// this couldve been an enum class
// but we need it to be easily castable to an integer
// + compactness
enum Register : std::uint8_t {
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
// connection to the debugger is not always present
#ifdef LAYER_DEBUG
  Debugee dbe{*this};
#endif

  bool cs, /* carry set */
      vs;  /* overflow set */
  bool mi, /* negative */
      z;   /* zero */

  std::array<reg_value_t, REG_COUNT> r = {
      0, 0,
      0, 0,
      0, 0,
      0, 0,
      0, 0,
      0, 0,
      0, LAYER_STACK_BASE + LAYER_STACK_SIZE - 1, // stack pointer
      0, 0,
  };

  std::array<uint8_t, LAYER_STACK_SIZE> stack = {0}; /* stack */
  std::vector<uint8_t> memory;                       /* memory */

  inline ExecutionState() { memory_init(); }
  inline virtual ~ExecutionState() {}

  inline ExecutionState(const ExecutionState &) = delete;
  inline ExecutionState &operator=(const ExecutionState &) = delete;

  // Memory
  void memory_init();
  void *memory_alloc(std::uint32_t size);
  void memory_free(void *p);

  // Addressing
  template <typename T> inline std::uint32_t address_map(T address) {
    static_assert(std::is_pointer_v<T>, "T must be a pointer!");
    return address_map(reinterpret_cast<std::uintptr_t>(address));
  }

  template <typename T> inline T address_resolve(std::uint32_t address) {
    static_assert(std::is_pointer_v<T>, "T must be a pointer!");
    return reinterpret_cast<T>(address_resolve(address));
  }

  virtual std::uint32_t address_map(std::uintptr_t address);
  virtual std::uintptr_t address_resolve(std::uint32_t address);

  // armv4.cpp

  // NOTE: op2_value is THE final value of operand2, either immediate or shifted
  // register (paired with op2_* calls)

  void arm_add(bool s, Register rd, Register rn, reg_value_t op2_value);
  void arm_adc(bool s, Register rd, Register rn, reg_value_t op2_value);
  void arm_sub(bool s, Register rd, Register rn, reg_value_t op2_value);
  void arm_sbc(bool s, Register rd, Register rn, reg_value_t op2_value);
  void arm_cmp(bool s, Register rd, Register rn, reg_value_t op2_value);
  void arm_mov(bool s, Register rd, Register rn, reg_value_t op2_value);
  void arm_rsb(bool s, Register rd, Register rn, reg_value_t op2_value);
  void arm_rsc(bool s, Register rd, Register rn, reg_value_t op2_value);
  void arm_and(bool s, Register rd, Register rn, reg_value_t op2_value);
  void arm_eor(bool s, Register rd, Register rn, reg_value_t op2_value);
  void arm_orr(bool s, Register rd, Register rn, reg_value_t op2_value);
  void arm_bic(bool s, Register rd, Register rn, reg_value_t op2_value);
  void arm_mvn(bool s, Register rd, Register rn, reg_value_t op2_value);
  void arm_tst(bool s, Register rd, Register rn, reg_value_t op2_value);
  void arm_teq(bool s, Register rd, Register rn, reg_value_t op2_value);
  void arm_cmn(bool s, Register rd, Register rn, reg_value_t op2_value);

  void arm_mul(bool s, Register rd, Register rn, Register rs, Register rm);
  void arm_mla(bool s, Register rd, Register rn, Register rs, Register rm);

  void arm_mull(bool s, bool sign, Register rd_hi, Register rd_lo, Register rs,
                Register rm);
  void arm_mlal(bool s, bool sign, Register rd_hi, Register rd_lo, Register rs,
                Register rm);

  void arm_ldr(bool pre_indx, bool add, bool byte, bool write_back, Register rn,
               Register rd, reg_value_t offset);
  void arm_str(bool pre_indx, bool add, bool byte, bool write_back, Register rn,
               Register rd, reg_value_t offset);
  void arm_ldm(bool pre_indx, bool add, bool write_back, Register rn,
               reg_value_t reg_list);
  void arm_stm(bool pre_indx, bool add, bool write_back, Register rn,
               reg_value_t reg_list);
  void arm_ldrh(bool pre_indx, bool add, bool write_back, Register rn,
                Register rd, uint8_t type, reg_value_t offset);
  void arm_strh(bool pre_indx, bool add, bool write_back, Register rn,
                Register rd, uint8_t type, reg_value_t offset);

  // TODO: implement armv5, add thumbv1

private:
  std::mutex _memory_lock;
};

} // namespace layer
