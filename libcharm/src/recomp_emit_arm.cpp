#include "libcharm/arm.hpp"
#include "libcharm/recomp.hpp"
#include <sstream>

const std::array<std::string, (int)charm::arm::Opcode::COUNT> OPCODE_TABLE = {
    "ps.arm_and", "ps.arm_eor", "ps.arm_sub", "ps.arm_rsb",
    "ps.arm_add", "ps.arm_adc", "ps.arm_sbc", "ps.arm_rsc",
    "ps.arm_tst", "ps.arm_teq", "ps.arm_cmp", "ps.arm_cmn",
    "ps.arm_orr", "ps.arm_mov", "ps.arm_bic", "ps.arm_mvn",
};

const std::array<std::string, (int)charm::arm::Register::COUNT> REGISTER_TABLE =
    {
        "R0", "R1", "R2",  "R3",  "R4",  "R5", "R6", "R7",
        "R8", "R9", "R10", "R11", "R12", "SP", "LR", "PC",
};

const std::array<std::string, (int)charm::arm::Condition::COUNT> COND_TABLE = {
    "EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC",
    "HI", "LS", "GE", "LT", "GT", "LE", "AL", "NV",
};

const std::array<std::string, 4> SHIFT_TABLE = {
    "op2_lsl",
    "op2_lsr",
    "op2_asr",
    "op2_ror",
};

namespace charm::recomp {

void Recompiler::emit_code_section(std::ofstream &ofs,
                                   const ELFIO::section *section) {
  if (!section) {
    return;
  }

  const auto data = section->get_data();
  const auto data_size = section->get_size();

  if (!data) {
    return;
  }

  std::cout << "\tSection " << section->get_name() << " ..." << std::endl;

  if (!_minify) {
    ofs << std::endl
        << "/* SECTION " << section->get_name() << " */" << std::endl;
  }

  std::stringstream ss;

  // actual emit
  for (arm::addr_t i = 0; i < data_size; i += sizeof(arm::instr_t)) {
    arm::addr_t addr = static_cast<arm::addr_t>(section->get_address() + i);

    ss << std::hex << "\tINSTR(0x" << addr << ") {" << std::dec << std::endl;

    arm::instr_t instr_raw;
    memcpy(&instr_raw, data + i, sizeof(arm::instr_t));
    auto instr = arm::Instruction::decode(instr_raw);

    // debug information for instruction debugging
    if (!_minify) {
      ss << "\t\t" << COND_TABLE[(int)instr.condition] << "(";
      ss << "LAYER_DBE_SKIP(ps, \"%s\", \"0x" << std::hex << addr << ": ";
      instr.dump(ss);
      ss << "\"));" << std::endl;
    }

    // emit instruction impl
    emit_arm(ss, instr, addr);

    ss << "\t\t[[fallthrough]];" << std::endl
       << "\t}" << std::endl
       << std::endl;
  }

  if (_minify) {
    std::string minimized_ss = ss.str();
    minimized_ss.erase(
        std::remove_if(minimized_ss.begin(), minimized_ss.end(),
                       [](char c) { return isspace(c) && c != ' '; }),
        minimized_ss.end());

    ofs << minimized_ss;
  } else {
    ofs << ss.rdbuf();
  }
}

void Recompiler::emit_arm(std::ostream &os, const arm::Instruction &instr,
                          arm::addr_t address) {
  if (instr.group == arm::InstructionGroup::INVALID) {
    if (_minify) {
      os << "\t\t__builtin_unreachable();";
    } else {
      os << "\t\tthrow std::runtime_error(\"Illegal instruction at 0x"
         << std::hex << address << std::dec << "\");";
    }
    return;
  }

  os << "\t\t" << COND_TABLE[(int)instr.condition] << "(";

  switch (instr.group) {
  case arm::InstructionGroup::DATA_PROCESSING: {
    emit_arm_data_processing(os, instr, address);
    break;
  }

  case arm::InstructionGroup::MULTIPLY:
    emit_arm_multiply(os, instr, address);
    break;

  case arm::InstructionGroup::MULTIPLY_LONG:
    emit_arm_multiply_long(os, instr, address);
    break;

  case arm::InstructionGroup::BRANCH: {
    emit_arm_branch(os, instr, address);
    break;
  }

  case arm::InstructionGroup::BRANCH_EXCHANGE:
    os << "JUMP(ps.r[" << REGISTER_TABLE[(int)instr.branchex.rm] << ");"
       << MINIFY_COMMENT(" /* bx */");
    break;

  case arm::InstructionGroup::SINGLE_DATA_SWAP:
    emit_arm_invalid(os, instr, address, "SWI is not implemented.");
    break;

  case arm::InstructionGroup::SINGLE_DATA_TRANSFER:
    emit_arm_single_data_transfer(os, instr, address);
    break;

  case arm::InstructionGroup::BLOCK_DATA_TRANSFER: {
    emit_arm_block_data_transfer(os, instr, address);
    break;
  }

  case arm::InstructionGroup::HALFWORD_DATA_TRANSFER:
    emit_arm_halfword_data_transfer(os, instr, address);
    break;

  case arm::InstructionGroup::SWI:
    emit_arm_invalid(os, instr, address, "SWI is not implemented.");
    break;

  default:
    emit_arm_invalid(os, instr, address, "Invalid instruction.");
    break;
  }

  emit_arm_modifies_pc(os, instr, address);
  os << ");" << std::endl;
}

void Recompiler::emit_arm_data_processing(std::ostream &os,
                                          const arm::Instruction &instr,
                                          arm::addr_t address) {
  if (instr.immediate) {
    os << OPCODE_TABLE[(int)instr.data.op] << "("
       << (instr.set_cflags ? "true" : "false")
       << MINIFY_COMMENT_COMMA(" /* set_cond */, ")

       << REGISTER_TABLE[(int)instr.data.rd]
       << MINIFY_COMMENT_COMMA(" /* rd */, ")

       << REGISTER_TABLE[(int)instr.data.rn]
       << MINIFY_COMMENT_COMMA(" /* rn */, ")

       << "0x" << std::hex << instr.data.op2_imm << std::dec
       << MINIFY_COMMENT(" /* op2_imm */") << ");";
    return;
  }

  if (instr.data.op2_reg.is_reg) {
    os << OPCODE_TABLE[(int)instr.data.op] << "("
       << (instr.set_cflags ? "true" : "false")
       << MINIFY_COMMENT_COMMA(" /* set_cond */, ")

       << REGISTER_TABLE[(int)instr.data.rd]
       << MINIFY_COMMENT_COMMA(" /* rd */, ")

       << REGISTER_TABLE[(int)instr.data.rn]
       << MINIFY_COMMENT_COMMA(" /* rn */, ")

       << SHIFT_TABLE[(int)instr.data.op2_reg.type] << "(ps, "
       << (instr.set_cflags ? "true" : "false") << ", ps.r["

       << REGISTER_TABLE[(int)instr.data.op2_reg.rm] << "]"
       << MINIFY_COMMENT_COMMA(" /* rm */, ")

       << "ps.r[" << REGISTER_TABLE[(int)instr.data.op2_reg.amount_or_rs] << "]"
       << MINIFY_COMMENT(" /* rs */") << "));";

  } else {
    os << OPCODE_TABLE[(int)instr.data.op] << "("
       << (instr.set_cflags ? "true" : "false")
       << MINIFY_COMMENT_COMMA(" /* set_cond */, ")

       << REGISTER_TABLE[(int)instr.data.rd]
       << MINIFY_COMMENT_COMMA(" /* rd */, ")

       << REGISTER_TABLE[(int)instr.data.rn]
       << MINIFY_COMMENT_COMMA(" /* rn */, ")

       << SHIFT_TABLE[(int)instr.data.op2_reg.type] << "(ps, "
       << (instr.set_cflags ? "true" : "false") << ", ps.r["

       << REGISTER_TABLE[(int)instr.data.op2_reg.rm] << "]"
       << MINIFY_COMMENT_COMMA(" /* rm */, ")

       << "0x" << std::hex << (uint32_t)instr.data.op2_reg.amount_or_rs
       << std::dec << MINIFY_COMMENT(" /* amount */") << "));";
  }
}

void Recompiler::emit_arm_multiply(std::ostream &os,
                                   const arm::Instruction &instr,
                                   arm::addr_t address) {
  os << (instr.mul.accumulate ? "ps.arm_mla" : "ps.arm_mul") << "("
     << (instr.set_cflags ? "true" : "false")
     << MINIFY_COMMENT_COMMA(" /* set_cond */, ")

     << REGISTER_TABLE[(int)instr.mul.rd] << MINIFY_COMMENT_COMMA(" /* rd */, ")

     << REGISTER_TABLE[(int)instr.mul.rn] << MINIFY_COMMENT_COMMA(" /* rn */, ")

     << REGISTER_TABLE[(int)instr.mul.rs] << MINIFY_COMMENT_COMMA(" /* rs */, ")

     << REGISTER_TABLE[(int)instr.mul.rm] << MINIFY_COMMENT(" /* rm */")
     << ");";
}

void Recompiler::emit_arm_multiply_long(std::ostream &os,
                                        const arm::Instruction &instr,
                                        arm::addr_t address) {
  os << (instr.mul_long.accumulate ? "ps.arm_mlal" : "ps.arm_mull") << "("
     << (instr.set_cflags ? "true" : "false")
     << MINIFY_COMMENT_COMMA(" /* set_cond */, ")

     << (instr.mul_long.sign ? "true" : "false")
     << MINIFY_COMMENT_COMMA(" /* sign */, ")

     << REGISTER_TABLE[(int)instr.mul_long.rd_lo]
     << MINIFY_COMMENT_COMMA(" /* rn_lo */, ")

     << REGISTER_TABLE[(int)instr.mul_long.rd_hi]
     << MINIFY_COMMENT_COMMA(" /* rd_hi */, ")

     << REGISTER_TABLE[(int)instr.mul_long.rm]
     << MINIFY_COMMENT_COMMA(" /* rm */, ")

     << REGISTER_TABLE[(int)instr.mul_long.rs] << MINIFY_COMMENT(" /* rs */")
     << ");";
}

void Recompiler::emit_arm_branch(std::ostream &os,
                                 const arm::Instruction &instr,
                                 arm::addr_t address) {
  std::uint32_t final_offset =
      (std::int64_t)(address + 8) + instr.branch.offset;

  // maybe we are calling external fn
  bool found_section = false;
  for (auto &section : _elf.sections) {
    if (!section_is_code(section.get())) {
      continue;
    }

    if (final_offset < section->get_address() ||
        final_offset >= section->get_address() + section->get_size()) {
      continue;
    }

    found_section = true;
    break;
  }

  if (!found_section) {
    emit_arm_invalid(os, instr, address,
                     "Attempt to branch to an invalid address: 0x%x",
                     final_offset);
    return;
  }

  if (instr.branch.link) {
    os << "ps.r[LR] = 0x" << std::hex << address + 4 << std::dec << "; ";
    os << "JUMP(" << std::hex << "0x" << final_offset << ");"
       << MINIFY_COMMENT("/* bl */");
    return;
  }

  // lets hope no one will raw branch into a function...
  os << "goto a0x" << std::hex << final_offset << std::dec << ";"
     << MINIFY_COMMENT(" /* b */");
}

void Recompiler::emit_arm_single_data_transfer(std::ostream &os,
                                               const arm::Instruction &instr,
                                               arm::addr_t address) {
  os << (instr.data_trans.load ? "ps.arm_ldr(" : "ps.arm_str(")

     << (instr.data_trans.pre_indx ? "true" : "false")
     << MINIFY_COMMENT_COMMA(" /* pre_indx */, ")

     << (instr.data_trans.add ? "true" : "false")
     << MINIFY_COMMENT_COMMA(" /* add */, ")

     << (instr.data_trans.byte ? "true" : "false")
     << MINIFY_COMMENT_COMMA(" /* byte */, ")

     << (instr.data_trans.write_back ? "true" : "false")
     << MINIFY_COMMENT_COMMA(" /* write_back */, ")

     << REGISTER_TABLE[(int)instr.data_trans.rn]
     << MINIFY_COMMENT_COMMA(" /* rn */, ")

     << REGISTER_TABLE[(int)instr.data_trans.rd]
     << MINIFY_COMMENT_COMMA(" /* rd */, ");

  os << std::hex;

  if (instr.immediate) {
    os << "0x" << (int)instr.data_trans.offset_imm
       << MINIFY_COMMENT(" /* offset */");
  } else {
    if (instr.data_trans.offset_reg.is_reg) {
      os << SHIFT_TABLE[(int)instr.data_trans.offset_reg.type]
         << "(ps, false, ps.r["
         << REGISTER_TABLE[(int)instr.data_trans.offset_reg.rm] << "]"
         << MINIFY_COMMENT_COMMA(" /* rm */,")

         << "ps.r["
         << REGISTER_TABLE[(int)instr.data_trans.offset_reg.amount_or_rs] << "]"
         << MINIFY_COMMENT(" /* rs */") << ")";
    } else {
      os << SHIFT_TABLE[(int)instr.data_trans.offset_reg.type]
         << "(ps, false, ps.r["
         << REGISTER_TABLE[(int)instr.data_trans.offset_reg.rm] << "]"
         << MINIFY_COMMENT_COMMA(" /* rm */,")

         << "0x" << std::hex
         << (std::uint32_t)instr.data_trans.offset_reg.amount_or_rs << std::dec
         << MINIFY_COMMENT(" /* amount */") << ")";
    }
  }

  os << std::dec;
  os << ");";
}

void Recompiler::emit_arm_halfword_data_transfer(std::ostream &os,
                                                 const arm::Instruction &instr,
                                                 arm::addr_t address) {
  os << (instr.hw_data_trans.load ? "ps.arm_ldrh(" : "ps.arm_strh(")
     << (instr.hw_data_trans.pre_indx ? "true" : "false")
     << MINIFY_COMMENT_COMMA(" /* pre_indx */, ")

     << (instr.hw_data_trans.add ? "true" : "false")
     << MINIFY_COMMENT_COMMA(" /* add */, ")

     << (instr.hw_data_trans.write_back ? "true" : "false")
     << MINIFY_COMMENT_COMMA(" /* write_back */, ")

     << REGISTER_TABLE[(int)instr.hw_data_trans.rn]
     << MINIFY_COMMENT_COMMA(" /* rn */, ")

     << REGISTER_TABLE[(int)instr.hw_data_trans.rd]
     << MINIFY_COMMENT_COMMA(" /* rd */, ")

     << "0x" << std::hex << (int)instr.hw_data_trans.type
     << MINIFY_COMMENT_COMMA(" /* type */, ");

  if (instr.immediate) {
    os << "0x" << (int)instr.hw_data_trans.offset_imm
       << MINIFY_COMMENT(" /* offset */");
  } else {
    os << REGISTER_TABLE[(int)instr.hw_data_trans.offset_reg]
       << MINIFY_COMMENT(" /* rm */");
  }

  // TODO: fix this

  os << std::dec << ");";
}

void Recompiler::emit_arm_block_data_transfer(std::ostream &os,
                                              const arm::Instruction &instr,
                                              arm::addr_t address) {
  os << (instr.blk_data_trans.load ? "ps.arm_ldm(" : "ps.arm_stm(")
     << (instr.blk_data_trans.pre_indx ? "true" : "false")
     << MINIFY_COMMENT_COMMA(" /* pre_indx */, ")

     << (instr.blk_data_trans.add ? "true" : "false")
     << MINIFY_COMMENT_COMMA(" /* add */, ")

     << (instr.blk_data_trans.write_back ? "true" : "false")
     << MINIFY_COMMENT_COMMA(" /* write_back */, ")

     << REGISTER_TABLE[(int)instr.blk_data_trans.rn]
     << MINIFY_COMMENT_COMMA(" /* rn */, ") << std::hex << "0x"

     << instr.blk_data_trans.reg_list << std::dec
     << MINIFY_COMMENT(" /* reg_list */") << ");";
}

// this function determines if the instruction modifies PC or not and generates
// code to emulate that behaviour
void Recompiler::emit_arm_modifies_pc(std::ostream &os,
                                      const arm::Instruction &instr,
                                      arm::addr_t address) {
  switch (instr.group) {
  case arm::InstructionGroup::DATA_PROCESSING: {
    // check if instruction tries to modify pc
    if (instr.data.rd != arm::Register::PC) {
      return;
    }

    switch (instr.data.op) {
    case arm::Opcode::TST:
    case arm::Opcode::TEQ:
    case arm::Opcode::CMP:
    case arm::Opcode::CMN:
    case arm::Opcode::COUNT:
    case arm::Opcode::INVALID:
      return; // these cant modify pc

    default:
      break; // remaining can
    }

    break;
  }

  case arm::InstructionGroup::MULTIPLY: {
    // rd must be pc
    if (instr.mul.rd != arm::Register::PC) {
      return;
    }

    break;
  }

  case arm::InstructionGroup::MULTIPLY_LONG: {
    // either rd hi or rd lo must be pc
    if (instr.mul_long.rd_hi != arm::Register::PC &&
        instr.mul_long.rd_lo != arm::Register::PC) {
      return;
    }

    break;
  }

  case arm::InstructionGroup::SINGLE_DATA_SWAP: {
    // rd must be pc
    if (instr.data_swap.rd != arm::Register::PC) {
      return;
    }

    break;
  }

    // In data transfer instructions, technically rn with writeback can be PC,
    // however this behaviour is unpredictable on most CPUs. As such, not
    // present here.

  case arm::InstructionGroup::SINGLE_DATA_TRANSFER: {
    if (!instr.data_trans.load) {
      return;
    }

    // rd must be pc
    if (instr.data_trans.rd != arm::Register::PC) {
      return;
    }

    break;
  }

  case arm::InstructionGroup::HALFWORD_DATA_TRANSFER: {
    if (!instr.hw_data_trans.load) {
      return;
    }

    if (instr.hw_data_trans.rd != arm::Register::PC) {
      return;
    }

    break;
  }

  case arm::InstructionGroup::BLOCK_DATA_TRANSFER: {
    if (!instr.blk_data_trans.load) {
      return;
    }

    if (!((instr.blk_data_trans.reg_list >> (int)arm::Register::PC) & 1)) {
      return;
    }

    break;
  }

  case arm::InstructionGroup::BRANCH_EXCHANGE:
  case arm::InstructionGroup::BRANCH:
  case arm::InstructionGroup::SWI:
  case arm::InstructionGroup::INVALID: {
    return; // these never modify pc, or the logic is handeled elsewhere (like
            // with branches)
  }
  }

  os << "JUMP(ps.r[" << REGISTER_TABLE[(int)arm::Register::PC] << ");";
  os << MINIFY_COMMENT(" /* modifies pc */");
}

} // namespace charm::recomp
