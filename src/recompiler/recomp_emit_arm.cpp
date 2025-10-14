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

const std::array<std::string, (int)isa::arm::Opcode::COUNT> OPCODE_TABLE = {
    "arm_and", "arm_eor", "arm_sub", "arm_rsb", "arm_add", "arm_adc",
    "arm_sbc", "arm_rsc", "arm_tst", "arm_teq", "arm_cmp", "arm_cmn",
    "arm_orr", "arm_mov", "arm_bic", "arm_mvn",
};

const std::array<std::string, (int)Register::COUNT> REGISTER_TABLE = {
    "R0", "R1", "R2",  "R3",  "R4",  "R5", "R6", "R7",
    "R8", "R9", "R10", "R11", "R12", "SP", "LR", "PC",
};

const std::array<std::string, 4> SHIFT_TABLE = {
    "op2_lsl",
    "op2_lsr",
    "op2_asr",
    "op2_ror",
};

void Recompiler::emit_code_section(std::ostream &os,
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
		if (!_minify) {
			ss << "\t\t" << COND_TABLE[(int)instr.condition] << "(";
			ss << "LAYER_DBE_SKIP(*this, \"%s\", \"0x" << std::hex << addr
			   << ": ";
			ss << instr.dump() << "\"));" << std::endl;
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

		os << minimized_ss;
	} else {
		os << ss.rdbuf();
	}
}

void Recompiler::emit_arm(std::ostream &os, const isa::arm::Instruction &instr,
                          Word address) {
	os << "\t\t" << COND_TABLE[(int)instr.condition] << "(";

	emit_arm_modifies_pc(os, instr, address);
	os << ");" << std::endl;
}

// this function determines if the instruction modifies PC or not and generates
// code to emulate that behaviour
void Recompiler::emit_arm_modifies_pc(std::ostream &os,
                                      const isa::arm::Instruction &instr,
                                      Word address) {
	switch ((isa::arm::InstructionGroup)instr.group.index()) {
	case isa::arm::InstructionGroup::DATA_PROCESSING: {
		const auto &data = std::get<isa::arm::DataProcessing>(instr.group);

		// check if instruction tries to modify pc
		if (data.rd != Register::PC) {
			return;
		}

		switch (data.op) {
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
		const auto &mul = std::get<isa::arm::Multiply>(instr.group);

		// rd must be pc
		if (mul.rd != Register::PC) {
			return;
		}

		break;
	}

	case isa::arm::InstructionGroup::MULTIPLY_LONG: {
		const auto &mul_long = std::get<isa::arm::MultiplyLong>(instr.group);

		// either rd hi or rd lo must be pc
		if (mul_long.rd_hi != Register::PC && mul_long.rd_lo != Register::PC) {
			return;
		}

		break;
	}

	case isa::arm::InstructionGroup::DATA_SWAP: {
		const auto &data_swap = std::get<isa::arm::DataSwap>(instr.group);

		// rd must be pc
		if (data_swap.rd != Register::PC) {
			return;
		}

		break;
	}

		// In data transfer instructions, technically rn with writeback can be
		// PC, however this behaviour is unpredictable on most CPUs. As such,
		// not present here.

	case isa::arm::InstructionGroup::DATA_TRANSFER: {
		const auto &data_trans = std::get<isa::arm::DataTransfer>(instr.group);
		if (!data_trans.ld) {
			return;
		}

		// rd must be pc
		if (data_trans.rd != Register::PC) {
			return;
		}

		break;
	}

	case isa::arm::InstructionGroup::HALFWORD_DATA_TRANSFER: {
		const auto &hw_data_trans =
		    std::get<isa::arm::HalfWordDataTransfer>(instr.group);
		if (!hw_data_trans.ld) {
			return;
		}

		if (hw_data_trans.rd != Register::PC) {
			return;
		}

		break;
	}

	case isa::arm::InstructionGroup::BLOCK_DATA_TRANSFER: {
		const auto &blk_data_trans =
		    std::get<isa::arm::BlockDataTransfer>(instr.group);
		if (!blk_data_trans.ld) {
			return;
		}

		if (!((blk_data_trans.reg_list >> Register::PC) & 1)) {
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
