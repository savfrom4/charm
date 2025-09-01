#include "libcharm/arm.hpp"
#include <array>

const std::array<std::string, (int)charm::arm::Opcode::COUNT> OPCODE_TABLE = {
    "and", "eor", "sub", "rsb", "add", "adc", "sbc", "rsc",
    "tst", "teq", "cmp", "cmn", "orr", "mov", "bic", "mvn",
};

const std::array<std::string, (int)charm::arm::Register::COUNT> REGISTER_TABLE =
    {
        "r0", "r1", "r2",  "r3",  "r4",  "r5", "r6", "r7",
        "r8", "r9", "r10", "r11", "r12", "sp", "lr", "pc",
};

const std::array<std::string, 4> SHIFT_TABLE = {
    "lsl",
    "lsr",
    "asr",
    "ror",
};

const std::array<std::string, (int)charm::arm::Condition::COUNT> COND_TABLE = {
    "eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
    "hi", "ls", "ge", "lt", "gt", "le", "al", "nv",
};

template <uint32_t n, uint32_t w = 1>
constexpr inline uint32_t get_bits(uint32_t x) {
  return ((x >> n) & ((1U << w) - 1));
}

template <uint32_t bits> inline int32_t sign_extend(uint32_t value) {
  if (value & (1 << (bits - 1))) {
    return value | (~0u << bits);
  } else {
    return value;
  }
}

namespace charm::arm {
Instruction Instruction::decode(instr_t instr) {
  Instruction info;
  info.raw = instr;
  info.cond = static_cast<Condition>(get_bits<28, 4>(instr));

  switch (get_bits<26, 2>(instr)) {
  // maybe data transfer
  case 0b00: {
    switch (get_bits<4, 4>(instr)) {

    // multiply / multiply long / single data swap
    case 0b1001: {
      uint32_t type = get_bits<23, 5>(instr);

      if (!type) { /* multiply */
        info.decode_multiply(instr);
        return info;
      } else if (type == 0b00001) { /* multiply long */
        info.decode_multiply_long(instr);
        return info;
      }

      // single data swap
      if (type == 0b00010 && !get_bits<8, 4>(instr)) {
        info.decode_single_data_swap(instr);
        return info;
      }
      break;
    }

    // branch exchange
    case 0b0001: {
      if (get_bits<4, 22>(instr) == 0b0100101111111111110001) {
        info.decode_branchex(instr);
        return info;
      }
    }
    }

    // half word transfer
    if (!get_bits<25>(instr) && get_bits<7>(instr) && get_bits<4>(instr)) {
      // immediate
      if (get_bits<22>(instr)) {
        info.decode_halfword_data_transfer(instr, true);
        return info;
      } else if (!get_bits<8, 4>(instr)) { // register
        info.decode_halfword_data_transfer(instr, false);
        return info;
      }
    }

    info.decode_data_processing(instr);
    break;
  }

  // single data transfer
  case 0b01: {
    info.decode_single_data_transfer(instr);
    break;
  }

  // branch / block data transfer
  case 0b10: {
    if (!get_bits<25>(instr)) {
      info.decode_block_data_transfer(instr);
    } else { // branch
      info.decode_branch(instr);
    }

    break;
  }

  // SWI
  case 0b11: {
    // those bits must be set for it to be SWI
    if (get_bits<24, 2>(instr) != 0b11) {
      break;
    }

    info.decode_swi(instr);
    break;
  }
  }

  return info;
}

// 4.5 Data Processing
inline void Instruction::decode_data_processing(instr_t instr) {
  group = InstructionGroup::DATA_PROCESSING;

  is_imm = get_bits<25>(instr); /* Immediate, bit 25 */

  data.op =
      static_cast<Opcode>(get_bits<21, 4>(instr)); /* Opcode, bits 21-24 */

  set_cond = get_bits<20>(instr); /* Set condition flags, bit 20 */
  data.rn = static_cast<Register>(
      get_bits<16, 4>(instr)); /* Rn register, bits 16-19 */
  data.rd = static_cast<Register>(
      get_bits<12, 4>(instr)); /* Rd register, bits 12-15 */

  // Operand 2
  if (is_imm) {
    uint32_t rotate =
        get_bits<8, 4>(instr); /* Amount to rotate by, bits 8-11 */

    uint32_t value = get_bits<0, 8>(instr); /* Value, bits 0-7 */

    rotate *= 2;
    data.op2_imm = (value >> rotate) | (value << (32 - rotate));
  } else {
    decode_shift(instr, data.op2_reg);
  }
}

// 4.7 Multiply and Multiply-Accumulate (MUL, MLA)
inline void Instruction::decode_multiply(instr_t instr) {
  group = InstructionGroup::MULTIPLY;

  mul.accumulate = get_bits<21>(instr); /* Accumulate, bit 21 */
  set_cond = get_bits<20>(instr);       /* Set condition flags, bit 20 */

  mul.rd = static_cast<Register>(
      get_bits<16, 4>(instr)); /* Rd register, bits 16-19 */
  mul.rn = static_cast<Register>(
      get_bits<12, 4>(instr)); /* Rn register, bits 16-19 */
  mul.rs =
      static_cast<Register>(get_bits<8, 4>(instr)); /* Rs register, bits 8-11 */
  mul.rm =
      static_cast<Register>(get_bits<0, 4>(instr)); /* Rm register, bits 0-3 */
}

// 4.8 Multiply Long and Multiply-Accumulate Long (MULL,MLAL)
inline void Instruction::decode_multiply_long(instr_t instr) {
  group = InstructionGroup::MULTIPLY_LONG;

  mul_long.sign = get_bits<22>(instr);       /* Unsigned, bit 22 */
  mul_long.accumulate = get_bits<21>(instr); /* Accumulate, bit 21 */
  set_cond = get_bits<20>(instr);            /* Set condition flags, bit 20 */

  mul_long.rd_hi = static_cast<Register>(
      get_bits<16, 4>(instr)); /* RdHi register, bits 16-19 */
  mul_long.rd_lo = static_cast<Register>(
      get_bits<12, 4>(instr)); /* RdLo register, bits 16-19 */
  mul_long.rs =
      static_cast<Register>(get_bits<8, 4>(instr)); /* Rs register, bits 8-11 */
  mul_long.rm =
      static_cast<Register>(get_bits<0, 4>(instr)); /* Rm register, bits 0-3 */
}

// 4.9 Single Data Transfer (LDR, STR)
inline void Instruction::decode_single_data_transfer(instr_t instr) {
  group = InstructionGroup::SINGLE_DATA_TRANSFER;

  is_imm = !get_bits<25>(instr);               /* Immediate, bit 25 */
  data_trans.pre_indx = get_bits<24>(instr);   /* Pre/Post indexing, bit 24 */
  data_trans.add = get_bits<23>(instr);        /* Up/Down, bit 23 */
  data_trans.byte = get_bits<22>(instr);       /* Byte/Word, bit 22 */
  data_trans.write_back = get_bits<21>(instr); /* Writeback, bit 21 */
  data_trans.load = get_bits<20>(instr);       /* Load/Store, bit 20 */

  data_trans.rn = static_cast<Register>(
      get_bits<16, 4>(instr)); /* Rn base register, bits 16-19 */

  data_trans.rd = static_cast<Register>(
      get_bits<12, 4>(instr)); /* Rd src/dst register, bits 12-15 */

  if (is_imm) {
    data_trans.offset_imm = get_bits<0, 12>(instr);
  } else {
    decode_shift(instr, data_trans.offset_reg);
  }
}

// 4.12 Single Data Swap (SWP)
inline void Instruction::decode_single_data_swap(instr_t instr) {
  group = InstructionGroup::SINGLE_DATA_SWAP;

  data_swap.byte = get_bits<22>(instr); /* Byte/Word, bit 22  */

  data_swap.rn = static_cast<Register>(
      get_bits<16, 4>(instr)); /* Rn base register, bits 16-19 */
  data_swap.rd = static_cast<Register>(
      get_bits<12, 4>(instr)); /* Rd dst register, bits 12-15 */
  data_swap.rm = static_cast<Register>(
      get_bits<0, 4>(instr)); /* Rm src register, bits 0-3 */
}

// 4.4 Branch and Branch with Link (B, BL)
inline void Instruction::decode_branch(instr_t instr) {
  group = InstructionGroup::BRANCH;

  branch.link = get_bits<24>(instr);            /* Link, bit 24 */
  uint32_t raw_offset = get_bits<0, 24>(instr); /* Offset, bits 0-23 */
  uint32_t shifted_offset = raw_offset << 2;
  branch.offset = sign_extend<26>(shifted_offset);
}

// 4.3 Branch and Exchange (BX)
inline void Instruction::decode_branchex(instr_t instr) {
  group = InstructionGroup::BRANCH_EXCHANGE;

  branchex.rm = static_cast<Register>(get_bits<0, 4>(instr)); /* Rn, bit 24 */
}

// 4.11 Block Data Transfer (LDM, STM)
inline void Instruction::decode_block_data_transfer(instr_t instr) {
  group = InstructionGroup::BLOCK_DATA_TRANSFER;

  blk_data_trans.pre_indx = get_bits<24>(instr); /* Pre/Post indexing, bit 24 */
  blk_data_trans.add = get_bits<23>(instr);      /* Up/Down, bit 23 */
  blk_data_trans.psr = get_bits<22>(instr);      /* PSR & Force user, bit 22 */
  blk_data_trans.write_back = get_bits<21>(instr); /* Writeback, bit 21 */
  blk_data_trans.load = get_bits<20>(instr);       /* Load/Store, bit 20 */
  blk_data_trans.rn =
      static_cast<Register>(get_bits<16, 4>(instr)); /* Rn, bits 16-19 */
  blk_data_trans.reg_list =
      get_bits<0, 16>(instr); /* Register list, bits 0-15 */
}

// 4.10 Halfword and Signed Data Transfer
inline void Instruction::decode_halfword_data_transfer(instr_t instr,
                                                       bool imm) {
  group = InstructionGroup::HALFWORD_DATA_TRANSFER;

  hw_data_trans.pre_indx = get_bits<24>(instr); /* Pre/Post indexing, bit 24 */
  hw_data_trans.add = get_bits<23>(instr);      /* Up/Down, bit 23 */
  hw_data_trans.write_back = get_bits<21>(instr); /* Writeback, bit 21 */
  hw_data_trans.load = get_bits<20>(instr);       /* Load/Store, bit 20 */
  hw_data_trans.rn =
      static_cast<Register>(get_bits<16, 4>(instr)); /* Rn, bits 16-19 */
  hw_data_trans.rd =
      static_cast<Register>(get_bits<12, 4>(instr)); /* Rd, bits 12-15 */

  hw_data_trans.type = static_cast<decltype(hw_data_trans.type)>(
      get_bits<5, 2>(instr)); /* Type, bits 5-6 */

  is_imm = imm;
  if (imm) {
    uint8_t offt_low = static_cast<uint16_t>(
        get_bits<0, 4>(instr)); /* Imm offset Low, bits 0-3 */
    uint8_t offt_high = static_cast<uint16_t>(
        get_bits<8, 4>(instr)); /* Imm offset High bits 8-11 */

    hw_data_trans.offset_imm =
        static_cast<uint8_t>((offt_high << 4) | offt_low);
  } else {
    hw_data_trans.rm =
        static_cast<Register>(get_bits<0, 4>(instr)); /* Rm, bits 0-3 */
  }
}

// 4.13 Software Interrupt (SWI)
inline void Instruction::decode_swi(instr_t instr) {
  group = InstructionGroup::SWI;

  /* we dont have to parse comment field */
}

// 4.5.2 Shifts
inline void Instruction::decode_shift(instr_t instr, Shifter &shift) {
  shift.type = static_cast<ShifterType>(get_bits<5, 2>(instr));

  shift.is_reg = get_bits<4>(instr); /* Shift register should be used, bit 4 */

  if (shift.is_reg) {
    shift.amount_or_rs = get_bits<8, 4>(instr); /* Rs register, bits 8-11 */
  } else {
    shift.amount_or_rs = get_bits<7, 5>(instr); /* Shift amount, bits 7-11 */
  }

  shift.rm =
      static_cast<Register>(get_bits<0, 4>(instr)); /* Rm register, bits 0-3 */
}

void Instruction::dump(std::ostream &ofs) {
  ofs << "(" << COND_TABLE[(int)cond] << ") ";

  switch (group) {
  case InstructionGroup::DATA_PROCESSING:
    ofs << OPCODE_TABLE[(int)data.op] << " ";
    ofs << REGISTER_TABLE[(int)data.rd] << ", " << REGISTER_TABLE[(int)data.rn]
        << ", ";

    if (is_imm) {
      ofs << "#" << (uint32_t)data.op2_imm;
    } else {
      ofs << REGISTER_TABLE[(int)data.op2_reg.rm];
      if (data.op2_reg.amount_or_rs != 0) {
        ofs << ", " << SHIFT_TABLE[(int)data.op2_reg.type] << " ";
        if (data.op2_reg.is_reg) {
          ofs << REGISTER_TABLE[data.op2_reg.amount_or_rs];
        } else {
          ofs << "#" << (int)data.op2_reg.amount_or_rs;
        }
      }
    }
    break;

  case InstructionGroup::MULTIPLY:
    ofs << (mul.accumulate ? "mla " : "mul ");
    ofs << REGISTER_TABLE[(int)mul.rd] << ", ";
    ofs << REGISTER_TABLE[(int)mul.rm] << ", ";
    ofs << REGISTER_TABLE[(int)mul.rs];

    if (mul.accumulate)
      ofs << ", " << REGISTER_TABLE[(int)mul.rn];

    break;

  case InstructionGroup::MULTIPLY_LONG:
    ofs << (mul_long.sign ? (mul_long.accumulate ? "smlal " : "smull ")
                          : (mul_long.accumulate ? "umlal " : "umull "));
    ofs << REGISTER_TABLE[(int)mul_long.rd_lo] << ", ";
    ofs << REGISTER_TABLE[(int)mul_long.rd_hi] << ", ";
    ofs << REGISTER_TABLE[(int)mul_long.rm] << ", ";
    ofs << REGISTER_TABLE[(int)mul_long.rs];
    break;

  case InstructionGroup::SINGLE_DATA_SWAP:
    ofs << (data_swap.byte ? "swpb " : "swp ");
    ofs << REGISTER_TABLE[(int)data_swap.rd] << ", ";
    ofs << REGISTER_TABLE[(int)data_swap.rm] << ", [";
    ofs << REGISTER_TABLE[(int)data_swap.rn] << "]";
    break;

  case InstructionGroup::BRANCH_EXCHANGE:
    ofs << "bx " << REGISTER_TABLE[(int)branchex.rm];
    break;

  case InstructionGroup::SINGLE_DATA_TRANSFER:
    ofs << (data_trans.load ? "ldr" : "str");

    if (data_trans.byte)
      ofs << "b";

    ofs << " " << REGISTER_TABLE[(int)data_trans.rd] << ", [";
    ofs << REGISTER_TABLE[(int)data_trans.rn];

    if (is_imm) {
      if (data_trans.offset_imm != 0)
        ofs << ", #" << (data_trans.add ? "" : "-")
            << (uint32_t)data_trans.offset_imm;
    } else {
      ofs << ", " << REGISTER_TABLE[(int)data_trans.offset_reg.rm];
      if (data_trans.offset_reg.amount_or_rs != 0) {
        ofs << ", " << SHIFT_TABLE[(int)data_trans.offset_reg.type] << " ";
        if (data_trans.offset_reg.is_reg) {
          ofs << REGISTER_TABLE[data_trans.offset_reg.amount_or_rs];
        } else {
          ofs << "#" << (int)data_trans.offset_reg.amount_or_rs;
        }
      }
    }

    ofs << "]";

    if (data_trans.write_back)
      ofs << "!";

    break;

  case InstructionGroup::BRANCH:
    ofs << (branch.link ? "bl " : "b ");
    ofs << "#" << branch.offset;
    break;

  case InstructionGroup::BLOCK_DATA_TRANSFER: {
    if (blk_data_trans.write_back && blk_data_trans.rn == Register::SP) {
      ofs << (blk_data_trans.load ? "pop" : "push");
    } else {
      ofs << (blk_data_trans.load ? "ldm" : "stm");
      ofs << " " << REGISTER_TABLE[(int)blk_data_trans.rn];
    }

    ofs << ", {";

    bool first = true;
    for (int i = 0; i < 16; ++i) {
      if (blk_data_trans.reg_list & (1 << i)) {

        if (!first)
          ofs << ", ";

        ofs << REGISTER_TABLE[i];
        first = false;
      }
    }

    ofs << "}";
    break;
  }
  case InstructionGroup::SWI:
    ofs << "swi";
    break;

  case InstructionGroup::INVALID:
    ofs << "invalid";
    break;

  case InstructionGroup::HALFWORD_DATA_TRANSFER:
    ofs << (hw_data_trans.load ? "ldr" : "str");

    switch (hw_data_trans.type) {
    case HalfWordTransferType::UHW:
      ofs << "h";
      break;
    case HalfWordTransferType::SWP:
      ofs << "swp";
      break;
    case HalfWordTransferType::SB:
      ofs << "sb";
      break;
    case HalfWordTransferType::SHW:
      ofs << "shr";
      break;
    }

    ofs << " " << REGISTER_TABLE[(int)hw_data_trans.rd] << ", [";
    ofs << REGISTER_TABLE[(int)hw_data_trans.rn];

    if (is_imm) {
      if (hw_data_trans.offset_imm != 0)
        ofs << ", #" << (uint32_t)hw_data_trans.offset_imm;
    } else {
      ofs << ", " << REGISTER_TABLE[(int)hw_data_trans.rm];
    }

    ofs << "]";

    if (data_trans.write_back)
      ofs << "!";

    break;
  }
}

} // namespace charm::arm
