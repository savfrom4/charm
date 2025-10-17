#include <sstream>

#include <isa/arm.hpp>
#include <recompiler/recomp.hpp>

// to automatically exclude comments
#define MINIFY_COMMENT(x) (_minify ? "" : x)
#define MINIFY_COMMENT_COMMA(x) (_minify ? "," : x)

namespace charm::recomp {

const std::array<std::string, (int)isa::arm::Condition::COUNT> COND_TABLE = {
    "EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC",
    "HI", "LS", "GE", "LT", "GT", "LE", "AL", "NV",
};

const std::array<std::string, (int)Register::COUNT> REGISTER_TABLE = {
    "R0", "R1", "R2",  "R3",  "R4",  "R5", "R6", "R7",
    "R8", "R9", "R10", "R11", "R12", "SP", "LR", "PC",
};

void Recompiler::_emit_code_section(std::ostream &os,
                                    const ELFIO::section *section) {
	const auto data = section->get_data();
	const auto data_size = section->get_size();

	std::cout << "\tSection " << section->get_name() << " ..." << std::endl;

	if (!_minify) {
		os << std::endl
		   << "\t/* SECTION " << section->get_name() << " */" << std::endl;
	}

	std::stringstream ss;

	// actual emit
	for (Word i = 0; i < data_size; i += sizeof(Word)) {
		Word addr = static_cast<Word>(section->get_address() + i);

		ss << std::hex << "\tINSTR(0x" << addr << ") {" << std::dec
		   << std::endl;

		Word instr_raw;
		std::memcpy(&instr_raw, data + i, sizeof(instr_raw));

		isa::arm::Instruction instr{instr_raw};

		// debug information for instruction debugging
		// if (!_minify) {
		// 	ss << "\t\t" << COND_TABLE[(int)instr.condition] << "(";
		// 	ss << "LAYER_DBE_SKIP(*this, \"%s\", \"0x" << std::hex << addr
		// 	   << ": ";
		// 	ss << instr.dump() << "\"));" << std::endl;
		// }

		// emit instruction impl
		_emit_arm(ss, instr, addr);

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

		os << minimized_ss;
	} else {
		os << ss.rdbuf();
	}
}

void Recompiler::_emit_arm(std::ostream &os, const isa::arm::Instruction &instr,
                           Word address) {
	os << "\t\t" << COND_TABLE[(int)instr.condition] << "(";

	switch (instr.group) {
	case isa::arm::InstructionGroup::DATA_PROCESSING: {
		os << _emit_arm_from_table<
		    std::array<std::string, (int)isa::arm::Opcode::COUNT>>(
		    {
		        "arm_and",
		        "arm_eor",
		        "arm_sub",
		        "arm_rsb",
		        "arm_add",
		        "arm_adc",
		        "arm_sbc",
		        "arm_rsc",
		        "arm_tst",
		        "arm_teq",
		        "arm_cmp",
		        "arm_cmn",
		        "arm_orr",
		        "arm_mov",
		        "arm_bic",
		        "arm_mvn",
		    },
		    (int)instr.data.op, instr.value);
		break;
	}

	case isa::arm::InstructionGroup::MULTIPLY: {
		os << _emit_arm_from_table<std::array<std::string, 2>>(
		    {
		        "arm_mul",
		        "arm_mla",
		    },
		    (int)instr.mul.a, instr.value);
		break;
	}

	case isa::arm::InstructionGroup::MULTIPLY_LONG: {
		os << _emit_arm_from_table<std::array<std::string, 2>>(
		    {
		        "arm_mull",
		        "arm_mlal",
		    },
		    (int)instr.mull.a, instr.value);
		break;
	}

	case isa::arm::InstructionGroup::DATA_TRANSFER: {
		os << _emit_arm_from_table<
		    std::array<std::string, (int)isa::arm::Opcode::COUNT>>(
		    {
		        "arm_ldr",
		        "arm_str",
		    },
		    (int)instr.data_trans.ld, instr.value, "memory");
		break;
	}

	case isa::arm::InstructionGroup::HALFWORD_DATA_TRANSFER: {

		os << _emit_arm_from_table<
		    std::array<std::string, (int)isa::arm::Opcode::COUNT>>(
		    {
		        "arm_ldrh",
		        "arm_strh",
		    },
		    (int)instr.hw_data_trans.ld, instr.value, "memory");
		break;
	}

	case isa::arm::InstructionGroup::BLOCK_DATA_TRANSFER: {
		os << _emit_arm_from_table<
		    std::array<std::string, (int)isa::arm::Opcode::COUNT>>(
		    {
		        "arm_ldm",
		        "arm_stm",
		    },
		    (int)instr.blk_data_trans.ld, instr.value, "memory");
		break;
	}

	case isa::arm::InstructionGroup::BRANCH: {
		Word final_offset = (std::int64_t)address + 8 + instr.branch.offset;

		// maybe we are calling external fn
		bool found_section = false;
		for (auto &section : _elf.sections) {
			if (!_section_is_code(section.get())) {
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
			os << _emit_arm_invalid(
			    address, instr.value,
			    "Attempt to branch to an invalid address: 0x%x", final_offset);
			break;
		}

		if (instr.branch.link) {
			os << "cpu.r[LR] = 0x" << std::hex << address + 4 << std::dec
			   << "; ";
			os << "address = " << std::hex << "0x" << final_offset
			   << "; goto __start__; " << MINIFY_COMMENT("/* bl */");
			break;
		}

		os << "goto a0x" << std::hex << final_offset << std::dec << "/* b */";
		break;
	}

	case isa::arm::InstructionGroup::BRANCH_EXCHANGE: {
		os << "address = " << std::hex << "cpu.r["
		   << REGISTER_TABLE[(int)instr.branchex.rm] << "]; goto __start__; "
		   << MINIFY_COMMENT("/* bx */");
		break;
	}

	case isa::arm::InstructionGroup::SWI: {
		os << _emit_arm_invalid(address, instr.value,
		                        "isa::arm::InstructionGroup::SWI");
		break;
	}

	case isa::arm::InstructionGroup::DATA_SWAP: {
		os << _emit_arm_invalid(address, instr.value,
		                        "isa::arm::InstructionGroup::DATA_SWAP");
		break;
	}

	case isa::arm::InstructionGroup::INVALID:
		break;
	}

	_emit_arm_modifies_pc(os, instr, address);
	os << ");" << std::endl;
}

// this function determines if the instruction modifies PC or not and generates
// code to emulate that behaviour
void Recompiler::_emit_arm_modifies_pc(std::ostream &os,
                                       const isa::arm::Instruction &instr,
                                       Word address) {
	switch (instr.group) {
	case isa::arm::InstructionGroup::DATA_PROCESSING: {
		// check if instruction tries to modify pc
		if (instr.data.rd != Register::PC) {
			return;
		}

		switch (instr.data.op) {
		case isa::arm::Opcode::TST:
		case isa::arm::Opcode::TEQ:
		case isa::arm::Opcode::CMP:
		case isa::arm::Opcode::CMN:
		case isa::arm::Opcode::COUNT:
			return; // these cant modify pc

		default:
			break; // remaining can
		}

		break;
	}

	case isa::arm::InstructionGroup::MULTIPLY: {
		// rd must be pc
		if (instr.mul.rd != Register::PC) {
			return;
		}

		break;
	}

	case isa::arm::InstructionGroup::MULTIPLY_LONG: {
		// either rd hi or rd lo must be pc
		if (instr.mull.rd_hi != Register::PC &&
		    instr.mull.rd_lo != Register::PC) {
			return;
		}

		break;
	}

	case isa::arm::InstructionGroup::DATA_SWAP: {
		// rd must be pc
		if (instr.data_swap.rd != Register::PC) {
			return;
		}

		break;
	}

		// In data transfer instructions, technically rn with writeback can be
		// PC, however this behaviour is unpredictable on most CPUs. As such,
		// not present here.

	case isa::arm::InstructionGroup::DATA_TRANSFER: {
		if (!instr.data_trans.ld) {
			return;
		}

		// rd must be pc
		if (instr.data_trans.rd != Register::PC) {
			return;
		}

		break;
	}

	case isa::arm::InstructionGroup::HALFWORD_DATA_TRANSFER: {
		if (!instr.hw_data_trans.ld) {
			return;
		}

		if (instr.hw_data_trans.rd != Register::PC) {
			return;
		}

		break;
	}

	case isa::arm::InstructionGroup::BLOCK_DATA_TRANSFER: {
		if (!instr.blk_data_trans.ld) {
			return;
		}

		if (!((instr.blk_data_trans.reg_list >> Register::PC) & 1)) {
			return;
		}

		break;
	}

	case isa::arm::InstructionGroup::BRANCH_EXCHANGE:
	case isa::arm::InstructionGroup::BRANCH:
	case isa::arm::InstructionGroup::SWI: {
	case isa::arm::InstructionGroup::INVALID: {
		return; // these never modify pc, or the logic is handeled elsewhere
		        // (like with branches)
	}
	}

		os << " JUMP(r[" << REGISTER_TABLE[Register::PC] << "]);";
		os << MINIFY_COMMENT(" /* modifies pc */");
	}
}

} // namespace charm::recomp
