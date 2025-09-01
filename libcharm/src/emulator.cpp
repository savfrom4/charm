// ----------- WARNING ! -------------
// This emulator is only used internaly to parse plt table.
// In the future it could be a standalone emulator, however for now it is not.
// -----------------------------------
#define LIBLAYER_IMPL
#include <liblayer/liblayer.hpp>

#include "libcharm/arm.hpp"
#include "libcharm/emulator.hpp"
#include <cstdint>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace charm {

reg_value_t shift(EmulationState &ps, charm::arm::Shifter shifter);

inline uint32_t EmulationState::address_map(uintptr_t addr) {
  for (auto &section : _elf->sections) {
    if (addr < reinterpret_cast<uintptr_t>(section->get_data()) ||
        addr >= reinterpret_cast<uintptr_t>(section->get_data()) +
                    section->get_size()) {
      continue;
    }

    uintptr_t offset = addr - reinterpret_cast<uintptr_t>(section->get_data());
    return static_cast<uint32_t>(section->get_address() + offset);
  }

  std::stringstream ss;
  ss << "Failed to map address: 0x" << std::hex << addr << std::dec
     << std::endl;

  throw std::runtime_error(ss.str());
}

inline uintptr_t EmulationState::address_resolve(uint32_t addr) {
  for (auto &section : _elf->sections) {
    if (addr < static_cast<uint32_t>(section->get_address()) ||
        addr >= static_cast<uint32_t>(section->get_address()) +
                    section->get_size()) {
      continue;
    }

    if (!section->get_data()) {
      std::cout << "invalid section " << section->get_name() << std::endl;
      return 0;
    }

    uintptr_t offset = addr - static_cast<uint32_t>(section->get_address());
    return reinterpret_cast<uintptr_t>(section->get_data()) + offset;
  }

  std::stringstream ss;
  ss << "Failed to resolve address: 0x" << std::hex << addr << std::dec
     << std::endl;

  throw std::runtime_error(ss.str());
}

Emulator::Emulator(ELFIO::elfio *elf, arm::addr_t address) {
  ps._elf = elf;
  set_address(address);
}

bool Emulator::step(arm::Instruction &instr) {
  const char *instr_addr = reinterpret_cast<const char *>(
      ps.address_resolve(ps.r[(int)arm::Register::PC] - 8));

  if (!instr_addr) {
    return false;
  }

  arm::instr_t raw_instr;
  memcpy(&raw_instr, instr_addr, sizeof(arm::instr_t));
  ps.r[(int)arm::Register::PC] += sizeof(arm::instr_t);

  instr = arm::Instruction::decode(raw_instr);
  if (!arm_check_cond(instr))
    return true;

  switch (instr.group) {
  case arm::InstructionGroup::DATA_PROCESSING: {
    arm_data_processing(instr);
    break;
  }

  case arm::InstructionGroup::MULTIPLY:
    if (instr.mul.accumulate) {
      ps.arm_mla(instr.set_cond, (reg_idx_t)instr.mul.rd,
                 (reg_idx_t)instr.mul.rn, (reg_idx_t)instr.mul.rs,
                 (reg_idx_t)instr.mul.rm);
    } else {
      ps.arm_mul(instr.set_cond, (reg_idx_t)instr.mul.rd,
                 (reg_idx_t)instr.mul.rn, (reg_idx_t)instr.mul.rs,
                 (reg_idx_t)instr.mul.rm);
    }
    break;

  /* Load / Store */
  case arm::InstructionGroup::SINGLE_DATA_TRANSFER: {
    if (!instr.data_trans.load) {
      ps.arm_str(instr.data_trans.pre_indx, instr.data_trans.add,
                 instr.data_trans.byte, instr.data_trans.write_back,
                 (reg_idx_t)instr.data_trans.rn, (reg_idx_t)instr.data.rd,
                 instr.is_imm ? instr.data_trans.offset_imm
                              : shift(ps, instr.data_trans.offset_reg),
                 false /* no copy */);
      break;
    }

    ps.arm_ldr(instr.data_trans.pre_indx, instr.data_trans.add,
               instr.data_trans.byte, instr.data_trans.write_back,
               (reg_idx_t)instr.data_trans.rn, (reg_idx_t)instr.data.rd,
               instr.is_imm ? instr.data_trans.offset_imm
                            : shift(ps, instr.data_trans.offset_reg),
               false /* no copy */);
    break;
  }

  default:
    break;

  case arm::InstructionGroup::INVALID:
    throw std::runtime_error("Invalid instruction");
  }

  return true;
}

/* This function checks if instruction condition is fulfilled or not. */
inline bool Emulator::arm_check_cond(const arm::Instruction &instr) {
  switch (instr.cond) {
  case arm::Condition::EQ:
    NE(return false;);
    break;

  case arm::Condition::NE:
    EQ(return false;);
    break;

  case arm::Condition::CS:
    CC(return false;);
    break;

  case arm::Condition::CC:
    CS(return false;);
    break;

  case arm::Condition::MI:
    PL(return false;);
    break;

  case arm::Condition::PL:
    MI(return false;);
    break;

  case arm::Condition::VS:
    VC(return false;);
    break;

  case arm::Condition::VC:
    VS(return false;);
    break;

  case arm::Condition::HI:
    LS(return false;);
    break;

  case arm::Condition::LS:
    HI(return false;);
    break;

  case arm::Condition::GE:
    LT(return false;);
    break;

  case arm::Condition::LT:
    GE(return false;);
    break;

  case arm::Condition::GT:
    LE(return false;);
    break;

  case arm::Condition::LE:
    GT(return false;);
    break;

  case arm::Condition::AL:
    break;

  case arm::Condition::NV:
    return false;

  default:
    throw std::runtime_error("Invalid condition");
  }

  return true;
}

/* This function handles data-processing ARM instructions. */
inline void Emulator::arm_data_processing(const arm::Instruction &instr) {
  switch (instr.data.op) {
  case arm::Opcode::AND:
    ps.arm_and(
        instr.set_cond, (reg_idx_t)instr.data.rd, (reg_idx_t)instr.data.rn,
        instr.is_imm ? instr.data.op2_imm : shift(ps, instr.data.op2_reg));
    break;

  case arm::Opcode::EOR:
    ps.arm_eor(
        instr.set_cond, (reg_idx_t)instr.data.rd, (reg_idx_t)instr.data.rn,
        instr.is_imm ? instr.data.op2_imm : shift(ps, instr.data.op2_reg));
    break;

  case arm::Opcode::SUB:
    ps.arm_sub(
        instr.set_cond, (reg_idx_t)instr.data.rd, (reg_idx_t)instr.data.rn,
        instr.is_imm ? instr.data.op2_imm : shift(ps, instr.data.op2_reg));
    break;

  case arm::Opcode::RSB:
    ps.arm_rsb(
        instr.set_cond, (reg_idx_t)instr.data.rd, (reg_idx_t)instr.data.rn,
        instr.is_imm ? instr.data.op2_imm : shift(ps, instr.data.op2_reg));
    break;

  case arm::Opcode::ADD:
    ps.arm_add(
        instr.set_cond, (reg_idx_t)instr.data.rd, (reg_idx_t)instr.data.rn,
        instr.is_imm ? instr.data.op2_imm : shift(ps, instr.data.op2_reg));
    break;

  case arm::Opcode::ADC:
    ps.arm_adc(
        instr.set_cond, (reg_idx_t)instr.data.rd, (reg_idx_t)instr.data.rn,
        instr.is_imm ? instr.data.op2_imm : shift(ps, instr.data.op2_reg));
    break;

  case arm::Opcode::SBC:
    ps.arm_sbc(
        instr.set_cond, (reg_idx_t)instr.data.rd, (reg_idx_t)instr.data.rn,
        instr.is_imm ? instr.data.op2_imm : shift(ps, instr.data.op2_reg));
    break;

  case arm::Opcode::RSC:
    ps.arm_rsc(
        instr.set_cond, (reg_idx_t)instr.data.rd, (reg_idx_t)instr.data.rn,
        instr.is_imm ? instr.data.op2_imm : shift(ps, instr.data.op2_reg));
    break;

  case arm::Opcode::TST:
    ps.arm_tst(
        instr.set_cond, (reg_idx_t)instr.data.rd, (reg_idx_t)instr.data.rn,
        instr.is_imm ? instr.data.op2_imm : shift(ps, instr.data.op2_reg));
    break;

  case arm::Opcode::TEQ:
    ps.arm_teq(
        instr.set_cond, (reg_idx_t)instr.data.rd, (reg_idx_t)instr.data.rn,
        instr.is_imm ? instr.data.op2_imm : shift(ps, instr.data.op2_reg));
    break;

  case arm::Opcode::CMP:
    ps.arm_cmp(
        instr.set_cond, (reg_idx_t)instr.data.rd, (reg_idx_t)instr.data.rn,
        instr.is_imm ? instr.data.op2_imm : shift(ps, instr.data.op2_reg));
    break;

  case arm::Opcode::CMN:
    ps.arm_cmn(
        instr.set_cond, (reg_idx_t)instr.data.rd, (reg_idx_t)instr.data.rn,
        instr.is_imm ? instr.data.op2_imm : shift(ps, instr.data.op2_reg));
    break;

  case arm::Opcode::ORR:
    ps.arm_orr(
        instr.set_cond, (reg_idx_t)instr.data.rd, (reg_idx_t)instr.data.rn,
        instr.is_imm ? instr.data.op2_imm : shift(ps, instr.data.op2_reg));
    break;

  case arm::Opcode::MOV:
    ps.arm_mov(
        instr.set_cond, (reg_idx_t)instr.data.rd, (reg_idx_t)instr.data.rn,
        instr.is_imm ? instr.data.op2_imm : shift(ps, instr.data.op2_reg));
    break;

  case arm::Opcode::BIC:
    ps.arm_bic(
        instr.set_cond, (reg_idx_t)instr.data.rd, (reg_idx_t)instr.data.rn,
        instr.is_imm ? instr.data.op2_imm : shift(ps, instr.data.op2_reg));
    break;

  case arm::Opcode::MVN:
    ps.arm_mvn(
        instr.set_cond, (reg_idx_t)instr.data.rd, (reg_idx_t)instr.data.rn,
        instr.is_imm ? instr.data.op2_imm : shift(ps, instr.data.op2_reg));
    break;

  case arm::Opcode::COUNT:
  case arm::Opcode::INVALID:
    break;
  }
}

inline reg_value_t shift(EmulationState &ps, charm::arm::Shifter shifter) {
  reg_value_t value = ps.r[(reg_idx_t)shifter.rm];
  reg_value_t amount =
      shifter.is_reg ? ps.r[shifter.amount_or_rs] : shifter.amount_or_rs;

  switch (shifter.type) {
  case charm::arm::ShifterType::LSL:
    return op2_lsl(value, amount);

  case charm::arm::ShifterType::LSR:
    return op2_lsr(value, amount);

  case charm::arm::ShifterType::ASR:
    return op2_asr(value, amount);

  case charm::arm::ShifterType::ROR:
    return op2_ror(value, amount);

  default: {
    break;
  }
  }

  __builtin_unreachable();
}

} // namespace charm
