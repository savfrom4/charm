/* ARM Documentation:
 * https://iitd-plos.github.io/col718/ref/arm-instructionset.pdf
 */

#pragma once
#include <cstdint>
#include <ostream>

namespace charm::arm {

// r0-15
enum class Register : uint8_t {
  // General-purpose

  R0,
  R1,
  R2,
  R3,
  R4,
  R5,
  R6,
  R7,
  R8,
  R9,
  R10,
  R11,

  IP, /* intra procedure call */
  SP, /* stack pointer */
  LR, /* link register */
  PC, /* program counter (instr_addr + 8) */
  COUNT,
};

enum class Opcode : uint8_t {
  AND = 0x0, /* logical and */
  EOR = 0x1, /* logical exclusive or */
  SUB = 0x2, /* subtract (no carry) */
  RSB = 0x3, /* reverse subtract (no carry) */
  ADD = 0x4, /* add (no carry) */
  ADC = 0x5, /* add (carry) */
  SBC = 0x6, /* subtract (carry) */
  RSC = 0x7, /* reverse subtract (carry) */
  TST = 0x8, /* test bits */
  TEQ = 0x9, /* test eql */
  CMP = 0xA, /* compare */
  CMN = 0xB, /* compare negative */
  ORR = 0xC, /* logical or */
  MOV = 0xD, /* move */
  BIC = 0xE, /* bit clear */
  MVN = 0xF, /* move not */

  COUNT,
  INVALID = 0xFF,
};

enum class Condition : uint8_t {
  EQ, /* equal */
  NE, /* not equal */
  CS, /* carry set */
  CC, /* carry clear */
  MI, /* negative */
  PL, /* positive or zero */
  VS, /* overflow set */
  VC, /* overflow clear */
  HI, /* unsigned higher */
  LS, /* unsigned lower or same */
  GE, /* signed greater or equal */
  LT, /* signed less than */
  GT, /* signed greater than */
  LE, /* signed less than */
  AL, /* always */
  NV, /* never */
  COUNT,
};

enum class InstructionGroup : uint8_t {
  DATA_PROCESSING,
  MULTIPLY,
  MULTIPLY_LONG,
  SINGLE_DATA_SWAP,
  BRANCH_EXCHANGE,
  HALFWORD_DATA_TRANSFER,
  SINGLE_DATA_TRANSFER,
  BLOCK_DATA_TRANSFER,
  BRANCH,
  SWI,

  INVALID = 0xFF,
};

enum class ShifterType : uint8_t {
  LSL,
  LSR,
  ASR,
  ROR,
};

enum class HalfWordTransferType : uint8_t {
  SWP = 0b00, /* SWP */
  UHW = 0b01, /* Unsigned half-word */
  SB = 0b10,  /* Signed byte */
  SHW = 0b11, /* Signed half-wrd */
};

struct Shifter {
  ShifterType type;
  Register rm; /* Rm register to shift. */

  bool is_reg;
  uint8_t amount_or_rs; /* Shift amount can be stored as an immediate
                           value or in a Rs register. */
};

struct Instruction {
public:
  inline Instruction() {}

  Condition cond = Condition::AL;
  InstructionGroup group = InstructionGroup::INVALID;
  bool is_imm, set_cond;

  union {
    struct {
      Opcode op = Opcode::INVALID;
      Register rd, rn;

      union {
        Shifter op2_reg;
        uint32_t op2_imm;
      };
    } data;

    struct {
      bool accumulate;
      Register rd, rn, rs, rm;
    } mul;

    struct {
      bool accumulate, sign /* Unsigned (0) or Signed (1) */;
      Register rd_hi, rd_lo; /* Low / high register to form a 32 bit value */
      Register rs, rm;
    } mul_long;

    struct {
      bool pre_indx; /* Add offset after (0) or before (1) transfer? */
      bool add;      /* Substract (0) or add (1) offset from base? */
      bool byte;
      bool write_back; /* Write address into base? */
      bool load;       /* Store (0) or Load (1)? */

      Register rn, rd;

      union {
        uint16_t offset_imm;
        Shifter offset_reg;
      };
    } data_trans;

    struct {
      bool pre_indx;   /* Add offset after (0) or before (1) transfer? */
      bool add;        /* Substract (0) or add (1) offset from base? */
      bool write_back; /* Write address into base? */
      bool load;       /* Store (0) or Load (1)? */

      Register rn, rd;
      HalfWordTransferType type;

      union {
        uint8_t offset_imm;
        Register rm;
      };

    } hw_data_trans;

    struct {
      bool byte;
      Register rn, rd, rm;
    } data_swap;

    struct {
      bool link;
      int32_t offset;
    } branch;

    struct {
      Register rm;
    } branchex;

    struct {
      bool pre_indx; /* Add offset after (0) or before (1) transfer? */
      bool add;      /* Substract (0) or add (1) offset from base? */
      bool psr;
      bool write_back; /* Write address into base? */
      bool load;       /* Store (0) or Load (1)? */

      Register rn;
      uint16_t reg_list;
    } blk_data_trans;
  };

  static Instruction decode(uint32_t instr);
  void dump(std::ostream &str);

private:
  void decode_data_processing(uint32_t instr);
  void decode_multiply(uint32_t instr);
  void decode_multiply_long(uint32_t instr);
  void decode_single_data_transfer(uint32_t instr);
  void decode_single_data_swap(uint32_t instr);
  void decode_branch(uint32_t instr);
  void decode_branchex(uint32_t instr);
  void decode_block_data_transfer(uint32_t instr);
  void decode_halfword_data_transfer(uint32_t instr, bool imm);
  void decode_swi(uint32_t instr);

  void decode_shift(uint32_t instr, Shifter &shift);
};

} // namespace charm::arm
