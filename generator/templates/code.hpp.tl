#pragma once
#include <cstdint>
#include <liblayer/liblayer.hpp>
#define INSTR_RETURN_LR (0xFFFFFFFF)

class ProgramState : public layer::ExecutionState {
  public:
	/* ADDRESS MAPPING */
	std::uint32_t address_map_raw(std::uintptr_t address) override;
	std::uintptr_t address_resolve_raw(std::uint32_t address) override;

	void eval(std::uint32_t address);

	// clang-format off

	// exported functions
/*% exported_functions %*/

    // external functions
/*% external_functions %*/

	// clang-format on
};
