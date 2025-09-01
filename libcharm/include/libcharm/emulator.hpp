// ----------- WARNING ! -------------
// This emulator is only used internaly to parse plt table.
// In the future it could be a standalone emulator, however for now it is not.
// -----------------------------------

#pragma once
#include "elfio/elfio.hpp"
#include "libcharm/arm.hpp"
#include <cstdint>
#include <liblayer/liblayer.hpp>

namespace charm {

class Emulator;

class EmulationState : public ExecutionState {
public:
  ELFIO::elfio *_elf;

  uint32_t address_map(uintptr_t addr) override;
  uintptr_t address_resolve(uint32_t addr) override;
};

class Emulator {
public:
  EmulationState ps;

  Emulator(ELFIO::elfio *elf, arm::addr_t address = 0);

  bool step(arm::Instruction &instr);

  inline void set_address(arm::addr_t addr) {
    ps.r[(int)arm::Register::PC] = addr + 8;
  }

  bool arm_check_cond(const arm::Instruction &instr);
  void arm_data_processing(const arm::Instruction &instr);
};
} // namespace charm
