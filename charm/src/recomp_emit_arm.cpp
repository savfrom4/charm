#include "arm.hpp"
#include "recomp.hpp"
#include <sstream>

// to automatically exclude comments
#define MINIFY_COMMENT(x) (_minify ? "" : x)
#define MINIFY_COMMENT_COMMA(x) (_minify ? "," : x)

const std::array<std::string, (int)charm::arm::Opcode::COUNT> OPCODE_TABLE = {
    "arm_and", "arm_eor", "arm_sub", "arm_rsb", "arm_add", "arm_adc",
    "arm_sbc", "arm_rsc", "arm_tst", "arm_teq", "arm_cmp", "arm_cmn",
    "arm_orr", "arm_mov", "arm_bic", "arm_mvn",
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
	for (arm::addr_t i = 0; i < data_size; i += sizeof(arm::instr_t)) {
		arm::addr_t addr = static_cast<arm::addr_t>(section->get_address() + i);

		ss << std::hex << "\tINSTR(0x" << addr << ") {" << std::dec
		   << std::endl;

		arm::instr_t instr_raw;
		std::memcpy(&instr_raw, data + i, sizeof(arm::instr_t));

		auto instr = arm::Instruction::decode(instr_raw);

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

void Recompiler::emit_arm(std::ostream &os, const arm::Instruction &instr,
                          arm::addr_t address) {
	os << "\t\t" << COND_TABLE[(int)instr.condition] << "(";

	switch ((arm::InstructionGroup)instr.group.index()) {
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

	case arm::InstructionGroup::BRANCH_EXCHANGE: {
		const auto &branchex = std::get<arm::BranchEx>(instr.group);
		os << "JUMP(r[" << REGISTER_TABLE[(int)branchex.rm] << "]);"
		   << MINIFY_COMMENT(" /* bx */");
		break;
	}

	case arm::InstructionGroup::DATA_SWAP:
		emit_arm_invalid(os, instr, address, "SWI is not implemented.");
		break;

	case arm::InstructionGroup::DATA_TRANSFER:
		emit_arm_data_transfer(os, instr, address);
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
	const auto &data = std::get<arm::DataProcessing>(instr.group);

	if (instr.immediate) {
		os << OPCODE_TABLE[(int)data.op] << "("
		   << (instr.set_cflags ? "true" : "false")
		   << MINIFY_COMMENT_COMMA(" /* s */, ")

		   << REGISTER_TABLE[(int)data.rd]
		   << MINIFY_COMMENT_COMMA(" /* rd */, ")

		   << REGISTER_TABLE[(int)data.rn]
		   << MINIFY_COMMENT_COMMA(" /* rn */, ")

		   << "0x" << std::hex << data.op2_imm << std::dec
		   << MINIFY_COMMENT(" /* op2_imm */") << ");";
		return;
	}

	if (data.op2_reg.is_reg) {
		os << OPCODE_TABLE[(int)data.op] << "("
		   << (instr.set_cflags ? "true" : "false")
		   << MINIFY_COMMENT_COMMA(" /* s */, ")

		   << REGISTER_TABLE[(int)data.rd]
		   << MINIFY_COMMENT_COMMA(" /* rd */, ")

		   << REGISTER_TABLE[(int)data.rn]
		   << MINIFY_COMMENT_COMMA(" /* rn */, ")

		   << SHIFT_TABLE[(int)data.op2_reg.type] << "(*this, "
		   << (instr.set_cflags ? "true" : "false") << ", r["

		   << REGISTER_TABLE[(int)data.op2_reg.rm] << "]"
		   << MINIFY_COMMENT_COMMA(" /* rm */, ")

		   << "r[" << REGISTER_TABLE[(int)data.op2_reg.amount_or_rs] << "]"
		   << MINIFY_COMMENT(" /* rs */") << "));";

	} else {
		os << OPCODE_TABLE[(int)data.op] << "("
		   << (instr.set_cflags ? "true" : "false")
		   << MINIFY_COMMENT_COMMA(" /* s */, ")

		   << REGISTER_TABLE[(int)data.rd]
		   << MINIFY_COMMENT_COMMA(" /* rd */, ")

		   << REGISTER_TABLE[(int)data.rn]
		   << MINIFY_COMMENT_COMMA(" /* rn */, ")

		   << SHIFT_TABLE[(int)data.op2_reg.type] << "(*this, "
		   << (instr.set_cflags ? "true" : "false") << ", r["

		   << REGISTER_TABLE[(int)data.op2_reg.rm] << "]"
		   << MINIFY_COMMENT_COMMA(" /* rm */, ")

		   << "0x" << std::hex << (uint32_t)data.op2_reg.amount_or_rs
		   << std::dec << MINIFY_COMMENT(" /* amount */") << "));";
	}
}

void Recompiler::emit_arm_multiply(std::ostream &os,
                                   const arm::Instruction &instr,
                                   arm::addr_t address) {
	const auto &mul = std::get<arm::Multiply>(instr.group);

	os << (mul.a ? "arm_mla" : "arm_mul") << "("
	   << (instr.set_cflags ? "true" : "false")
	   << MINIFY_COMMENT_COMMA(" /* s */, ")

	   << REGISTER_TABLE[(int)mul.rd] << MINIFY_COMMENT_COMMA(" /* rd */, ")
	   << REGISTER_TABLE[(int)mul.rn] << MINIFY_COMMENT_COMMA(" /* rn */, ")
	   << REGISTER_TABLE[(int)mul.rs] << MINIFY_COMMENT_COMMA(" /* rs */, ")
	   << REGISTER_TABLE[(int)mul.rm] << MINIFY_COMMENT(" /* rm */") << ");";
}

void Recompiler::emit_arm_multiply_long(std::ostream &os,
                                        const arm::Instruction &instr,
                                        arm::addr_t address) {
	const auto &mul_long = std::get<arm::MultiplyLong>(instr.group);

	os << (mul_long.a ? "arm_mlal" : "arm_mull") << "("
	   << (instr.set_cflags ? "true" : "false")
	   << MINIFY_COMMENT_COMMA(" /* s */, ")

	   << (mul_long.sign ? "true" : "false")
	   << MINIFY_COMMENT_COMMA(" /* sign */, ")

	   << REGISTER_TABLE[(int)mul_long.rd_lo]
	   << MINIFY_COMMENT_COMMA(" /* rn_lo */, ")

	   << REGISTER_TABLE[(int)mul_long.rd_hi]
	   << MINIFY_COMMENT_COMMA(" /* rd_hi */, ")

	   << REGISTER_TABLE[(int)mul_long.rm]
	   << MINIFY_COMMENT_COMMA(" /* rm */, ")
	   << REGISTER_TABLE[(int)mul_long.rs] << MINIFY_COMMENT(" /* rs */")
	   << ");";
}

void Recompiler::emit_arm_branch(std::ostream &os,
                                 const arm::Instruction &instr,
                                 arm::addr_t address) {
	const auto &branch = std::get<arm::Branch>(instr.group);
	std::uint32_t final_offset = (std::int64_t)(address + 8) + branch.offset;

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

	if (branch.link) {
		os << "r[LR] = 0x" << std::hex << address + 4 << std::dec << "; ";
		os << "JUMP(" << std::hex << "0x" << final_offset << ");"
		   << MINIFY_COMMENT("/* bl */");
		return;
	}

	// lets hope no one will raw branch into a function...
	os << "goto a0x" << std::hex << final_offset << std::dec << ";"
	   << MINIFY_COMMENT(" /* b */");
}

void Recompiler::emit_arm_data_transfer(std::ostream &os,
                                        const arm::Instruction &instr,
                                        arm::addr_t address) {
	const auto &data_trans = std::get<arm::DataTransfer>(instr.group);

	os << (data_trans.load ? "arm_ldr(" : "arm_str(")

	   << (data_trans.p ? "true" : "false")
	   << MINIFY_COMMENT_COMMA(" /* p */, ")

	   << (data_trans.u ? "true" : "false")
	   << MINIFY_COMMENT_COMMA(" /* u */, ")

	   << (data_trans.b ? "true" : "false")
	   << MINIFY_COMMENT_COMMA(" /* b */, ")

	   << (data_trans.w ? "true" : "false")
	   << MINIFY_COMMENT_COMMA(" /* w */, ")

	   << REGISTER_TABLE[(int)data_trans.rn]
	   << MINIFY_COMMENT_COMMA(" /* rn */, ")

	   << REGISTER_TABLE[(int)data_trans.rd]
	   << MINIFY_COMMENT_COMMA(" /* rd */, ");

	os << std::hex;

	if (instr.immediate) {
		os << "0x" << (int)data_trans.imm << MINIFY_COMMENT(" /* offset */");
	} else {
		if (data_trans.reg.is_reg) {
			os << SHIFT_TABLE[(int)data_trans.reg.type] << "(*this, false, r["
			   << REGISTER_TABLE[(int)data_trans.reg.rm] << "]"
			   << MINIFY_COMMENT_COMMA(" /* rm */,")

			   << "r[" << REGISTER_TABLE[(int)data_trans.reg.amount_or_rs]
			   << "]" << MINIFY_COMMENT(" /* rs */") << ")";
		} else {
			os << SHIFT_TABLE[(int)data_trans.reg.type] << "(*this, false, r["
			   << REGISTER_TABLE[(int)data_trans.reg.rm] << "]"
			   << MINIFY_COMMENT_COMMA(" /* rm */,")

			   << "0x" << std::hex << (std::uint32_t)data_trans.reg.amount_or_rs
			   << std::dec << MINIFY_COMMENT(" /* amount */") << ")";
		}
	}

	os << std::dec;
	os << ");";
}

void Recompiler::emit_arm_halfword_data_transfer(std::ostream &os,
                                                 const arm::Instruction &instr,
                                                 arm::addr_t address) {
	const auto &hw_data_trans =
	    std::get<arm::HalfWordDataTransfer>(instr.group);

	os << (hw_data_trans.ld ? "arm_ldrh(" : "arm_strh(")
	   << (hw_data_trans.p ? "true" : "false")
	   << MINIFY_COMMENT_COMMA(" /* p */, ")

	   << (hw_data_trans.u ? "true" : "false")
	   << MINIFY_COMMENT_COMMA(" /* u */, ")

	   << (hw_data_trans.w ? "true" : "false")
	   << MINIFY_COMMENT_COMMA(" /* w */, ")

	   << REGISTER_TABLE[(int)hw_data_trans.rn]
	   << MINIFY_COMMENT_COMMA(" /* rn */, ")

	   << REGISTER_TABLE[(int)hw_data_trans.rd]
	   << MINIFY_COMMENT_COMMA(" /* rd */, ")

	   << "0x" << std::hex << (int)hw_data_trans.type
	   << MINIFY_COMMENT_COMMA(" /* type */, ");

	if (instr.immediate) {
		os << "0x" << (int)hw_data_trans.imm << MINIFY_COMMENT(" /* offset */");
	} else {
		os << "r[" << REGISTER_TABLE[(int)hw_data_trans.rm] << "]"
		   << MINIFY_COMMENT(" /* rm */");
	}

	os << std::dec << ");";
}

void Recompiler::emit_arm_block_data_transfer(std::ostream &os,
                                              const arm::Instruction &instr,
                                              arm::addr_t address) {
	const auto &blk_data_trans = std::get<arm::BlockDataTransfer>(instr.group);
	os << (blk_data_trans.ld ? "arm_ldm(" : "arm_stm(")
	   << (blk_data_trans.p ? "true" : "false")
	   << MINIFY_COMMENT_COMMA(" /* p */, ")

	   << (blk_data_trans.u ? "true" : "false")
	   << MINIFY_COMMENT_COMMA(" /* u */, ")

	   << (blk_data_trans.w ? "true" : "false")
	   << MINIFY_COMMENT_COMMA(" /* w */, ")

	   << REGISTER_TABLE[(int)blk_data_trans.rn]
	   << MINIFY_COMMENT_COMMA(" /* rn */, ") << std::hex << "0x"

	   << blk_data_trans.reg_list << std::dec
	   << MINIFY_COMMENT(" /* reg_list */") << ");";
}

// this function determines if the instruction modifies PC or not and generates
// code to emulate that behaviour
void Recompiler::emit_arm_modifies_pc(std::ostream &os,
                                      const arm::Instruction &instr,
                                      arm::addr_t address) {
	switch ((arm::InstructionGroup)instr.group.index()) {
	case arm::InstructionGroup::DATA_PROCESSING: {
		const auto &data = std::get<arm::DataProcessing>(instr.group);

		// check if instruction tries to modify pc
		if (data.rd != arm::Register::PC) {
			return;
		}

		switch (data.op) {
		case arm::Opcode::TST:
		case arm::Opcode::TEQ:
		case arm::Opcode::CMP:
		case arm::Opcode::CMN:
		case arm::Opcode::COUNT:
			return; // these cant modify pc

		default:
			break; // remaining can
		}

		break;
	}

	case arm::InstructionGroup::MULTIPLY: {
		const auto &mul = std::get<arm::Multiply>(instr.group);

		// rd must be pc
		if (mul.rd != arm::Register::PC) {
			return;
		}

		break;
	}

	case arm::InstructionGroup::MULTIPLY_LONG: {
		const auto &mul_long = std::get<arm::MultiplyLong>(instr.group);

		// either rd hi or rd lo must be pc
		if (mul_long.rd_hi != arm::Register::PC &&
		    mul_long.rd_lo != arm::Register::PC) {
			return;
		}

		break;
	}

	case arm::InstructionGroup::DATA_SWAP: {
		const auto &data_swap = std::get<arm::DataSwap>(instr.group);

		// rd must be pc
		if (data_swap.rd != arm::Register::PC) {
			return;
		}

		break;
	}

		// In data transfer instructions, technically rn with writeback can be
		// PC, however this behaviour is unpredictable on most CPUs. As such,
		// not present here.

	case arm::InstructionGroup::DATA_TRANSFER: {
		const auto &data_trans = std::get<arm::DataTransfer>(instr.group);
		if (!data_trans.load) {
			return;
		}

		// rd must be pc
		if (data_trans.rd != arm::Register::PC) {
			return;
		}

		break;
	}

	case arm::InstructionGroup::HALFWORD_DATA_TRANSFER: {
		const auto &hw_data_trans =
		    std::get<arm::HalfWordDataTransfer>(instr.group);
		if (!hw_data_trans.ld) {
			return;
		}

		if (hw_data_trans.rd != arm::Register::PC) {
			return;
		}

		break;
	}

	case arm::InstructionGroup::BLOCK_DATA_TRANSFER: {
		const auto &blk_data_trans =
		    std::get<arm::BlockDataTransfer>(instr.group);
		if (!blk_data_trans.ld) {
			return;
		}

		if (!((blk_data_trans.reg_list >> (int)arm::Register::PC) & 1)) {
			return;
		}

		break;
	}

	case arm::InstructionGroup::BRANCH_EXCHANGE:
	case arm::InstructionGroup::BRANCH:
	case arm::InstructionGroup::SWI: {
		return; // these never modify pc, or the logic is handeled elsewhere
		        // (like with branches)
	}
	}

	os << " JUMP(r[" << REGISTER_TABLE[(int)arm::Register::PC] << "]);";
	os << MINIFY_COMMENT(" /* modifies pc */");
}

} // namespace charm::recomp
