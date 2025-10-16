#pragma once
#include <cstdint>

#include <arch.hpp>
#include <runtime/cpu.hpp>
#include <runtime/memory.hpp>

#define INSTR_RETURN_LR (0xFFFFFFFF)

using namespace charm::runtime;

struct ExecutionState {
	CPUState cpu;

	Memory memory{
	    .elf_size = /*% mem_elf_size %*/,
	    .heap_size = 64 * 1024 * 1024, // 64 MiB
	    .heap_blk_size = 64,           // 64 bytes
	    .stack_size = 4 * 1024 * 1024, // 4 MiB
	};

	inline ExecutionState() {
		const auto &config = memory.config();
		cpu.r[SP] = memory.stack_base() + config.stack_size;

		init_elf_sections();
	}

	void init_elf_sections();
	void eval(Word address);

	// clang-format off

	// exported functions
/*% exported_functions %*/

    // external functions
/*% external_functions %*/

	// clang-format on
};
