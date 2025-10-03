/* ARM Documentation:
 * https://iitd-plos.github.io/col718/ref/arm-instructionset.pdf
 */

#pragma once
#include <cstdint>
#include <ostream>

namespace charm::arm {
typedef uint32_t addr_t;
typedef uint32_t instr_t;

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
  AND, /* logical and */
  EOR, /* logical exclusive or */
  SUB, /* substract (no carry) */
  RSB, /* reverse substract (no carry) */
  ADD, /* add (no carry) */
  ADC, /* add (carry) */
  SBC, /* substract (carry) */
  RSC, /* reverse substract (carry) */
  TST, /* test bits */
  TEQ, /* test eql */
  CMP, /* compare */
  CMN, /* compare negative */
  ORR, /* logical or */
  MOV, /* move */
  BIC, /* bit clear */
  MVN, /* move not */

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
  SINGLE_DATA_TRANSFER,
  HALFWORD_DATA_TRANSFER,
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
  SHW = 0b11, /* Signed half-word */
};

struct Shifter {
  ShifterType type;
  Register rm; /* Rm register to shift. */

  bool is_reg;
  uint8_t amount_or_rs; /* Shift amount can be stored as an immediate
                           value or in a Rs register. */
};

class Instruction {
public:
  inline Instruction() {}

  Condition condition = Condition::AL;
  InstructionGroup group = InstructionGroup::INVALID;
  bool immediate /*  is operand immediate or register? */,
      set_cflags /* will set condition flags ? (not required by
                    all instructions) */
      ;

  union {
    struct {
      Opcode op = Opcode::INVALID;
      Register rd, rn;

      union {
        Shifter op2_reg;
        uint32_t op2_imm;
      }; // operand 2
    } data;

    struct {
      bool accumulate;
      Register rd, rn, rs, rm;
    } mul;

    struct {
      bool accumulate, sign /* unsigned (0) or signed (1) */;
      Register rd_hi, rd_lo; /* low / high register to form a 32 bit value */
      Register rs, rm;
    } mul_long;

    struct {
      bool pre_indx; /* add offset after (0) or before (1) transfer? */
      bool add;      /* substract (0) or add (1) offset from base? */
      bool byte;
      bool write_back; /* write address into base? */
      bool load;       /* store (0) or Load (1)? */

      Register rn, rd;

      union {
        Shifter offset_reg;
        uint16_t offset_imm;
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
        Register offset_reg;
        uint8_t offset_imm;
      };

    } hw_data_trans;

    struct {
      bool pre_indx; /* Add offset after (0) or before (1) transfer? */
      bool add;      /* Substract (0) or add (1) offset from base? */
      bool psr;
      bool write_back; /* Write address into base? */
      bool load;       /* Store (0) or Load (1)? */

      Register rn;
      uint16_t reg_list;
    } blk_data_trans;

    struct {
      bool byte;
      Register rn, rd, rm;
    } data_swap;

    struct {
      bool link;
      int32_t offset; // NOTE: signed offset, be careful!
    } branch;

    struct {
      Register rm;
    } branchex;
  };

  instr_t raw; /* raw representation of the instruction */

  static Instruction decode(instr_t instr);
  void dump(std::ostream &str);

private:
  void decode_data_processing(instr_t instr);
  void decode_multiply(instr_t instr);
  void decode_multiply_long(instr_t instr);
  void decode_branch(instr_t instr);
  void decode_branchex(instr_t instr);
  void decode_single_data_swap(instr_t instr);
  void decode_single_data_transfer(instr_t instr);
  void decode_halfword_data_transfer(instr_t instr, bool imm);
  void decode_block_data_transfer(instr_t instr);
  void decode_swi(instr_t instr);

  void decode_shift(instr_t instr, Shifter &shift);
};

} // namespace charm::arm
