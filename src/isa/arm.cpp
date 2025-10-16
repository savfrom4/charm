#include <array>
#include <inttypes.h>
#include <sstream>
#include <string>

#include <isa/arm.hpp>
#include <utils.hpp>

namespace charm::isa::arm {

const std::array<std::string, Register::COUNT> REGISTER_TABLE = {
    "r0", "r1", "r2",  "r3",  "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11", "ip", "sp", "lr", "pc",
};

const std::array<std::string, (int)Condition::COUNT> COND_TABLE = {
    "eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
    "hi", "ls", "ge", "lt", "gt", "le", "al", "nv",
};

const std::array<std::string, (int)Opcode::COUNT> OPCODE_TABLE = {
    "and", "eor", "sub", "rsb", "add", "adc", "sbc", "rsc",
    "tst", "teq", "cmp", "cmn", "orr", "mov", "bic", "mvn",
};

const std::array<std::string, 4> SHIFT_TABLE = {
    "lsl",
    "lsr",
    "asr",
    "ror",
};

std::string Instruction::dump() const {
	std::stringstream ss;

	// add condition prefix
	ss << utils::sformat("(%s) ", COND_TABLE[(int)condition]);

	switch ((InstructionGroup)group.index()) {

	case InstructionGroup::DATA_PROCESSING: {
		const auto &data = std::get<DataProcessing>(group);

		// opcode, rd, rn
		ss << utils::sformat("%s\t%s, %s, ", OPCODE_TABLE[(int)data.op],
		                     REGISTER_TABLE[(int)data.rd],
		                     REGISTER_TABLE[(int)data.rn]);

		if (is_imm) { // #imm
			ss << utils::sformat("#%" PRIu32, data.op2_imm);
		} else { // rm
			ss << REGISTER_TABLE[(int)data.op2_reg.rm];

			// , shift
			if (data.op2_reg.amount_or_rs != 0) {
				ss << utils::sformat(
				    ", %s %s", SHIFT_TABLE[(int)data.op2_reg.type],
				    (data.op2_reg.is_reg
				         ? REGISTER_TABLE[(int)data.op2_reg.amount_or_rs]
				         : utils::sformat("#%" PRIu8,
				                          data.op2_reg.amount_or_rs)));
			}
		}

		break;
	}

	case InstructionGroup::MULTIPLY: {
		const auto &mul = std::get<Multiply>(group);

		// mul/mla rd, rm, rs
		ss << utils::sformat("%s\t%s, %s, %s", (mul.a ? "mla" : "mul"),
		                     REGISTER_TABLE[(int)mul.rd],
		                     REGISTER_TABLE[(int)mul.rm],
		                     REGISTER_TABLE[(int)mul.rs]);

		// , rn
		if (mul.a) {
			ss << ", " << REGISTER_TABLE[(int)mul.rn];
		}

		break;
	}

	case InstructionGroup::MULTIPLY_LONG: {
		const auto &mul_long = std::get<MultiplyLong>(group);

		// prefix
		ss << (mul_long.sign ? "s" : "u");

		// (s/u)mul/mlal rd_lo, rd_hi, rm, rs
		ss << utils::sformat("%s\t%s, %s, %s, %s", mul_long.a ? "mlal" : "mull",
		                     REGISTER_TABLE[(int)mul_long.rd_lo],
		                     REGISTER_TABLE[(int)mul_long.rd_hi],
		                     REGISTER_TABLE[(int)mul_long.rm],
		                     REGISTER_TABLE[(int)mul_long.rs]);
		break;
	}

	case InstructionGroup::DATA_TRANSFER: {
		const auto &data_trans = std::get<DataTransfer>(group);

		// push/pop rd
		if (is_imm && data_trans.w && data_trans.rn == Register::SP &&
		    data_trans.imm == 4 && data_trans.u == data_trans.ld) {
			ss << utils::sformat("%s\t{%s}", data_trans.ld ? "pop" : "push",
			                     REGISTER_TABLE[(int)data_trans.rd]);
			break;
		}

		// ldr/str(b) rd, [rn
		ss << utils::sformat("%s%s\t%s, [%s", data_trans.ld ? "ldr" : "str",
		                     data_trans.b ? "b" : "",
		                     REGISTER_TABLE[(int)data_trans.rd],
		                     REGISTER_TABLE[(int)data_trans.rn]);

		// close the square bracket if post indexed
		if (!data_trans.p) {
			ss << "]";
		}

		if (is_imm) {
			//, #imm
			ss << utils::sformat(", #%s%" PRIu16, data_trans.u ? "" : "-",
			                     data_trans.imm);
		} else {
			ss << REGISTER_TABLE[(int)data_trans.reg.rm];

			// , shift
			if (data_trans.reg.amount_or_rs != 0) {
				ss << utils::sformat(
				    ", %s%s", SHIFT_TABLE[(int)data_trans.reg.type],
				    (data_trans.reg.is_reg
				         ? utils::sformat(
				               ", %s",
				               REGISTER_TABLE[data_trans.reg.amount_or_rs])
				         : utils::sformat(" #%" PRIu8,
				                          data_trans.reg.amount_or_rs)));
			}
		}

		if (data_trans.p) {
			ss << "]";

			if (data_trans.w) {
				ss << "!";
			}
		}

		break;
	}

	case InstructionGroup::HALFWORD_DATA_TRANSFER: {
		const auto &hw_data_trans = std::get<HalfWordDataTransfer>(group);

		const std::array<std::string, 4> type_table = {
		    "(INVALID)",
		    "h",
		    "sb",
		    "shw",
		};

		ss << utils::sformat("%s%s\t%s, [%s", hw_data_trans.ld ? "ldr" : "str",
		                     type_table[(int)hw_data_trans.type],
		                     REGISTER_TABLE[(int)hw_data_trans.rd],
		                     REGISTER_TABLE[(int)hw_data_trans.rn]);

		// close the square bracket if post indexed
		if (!hw_data_trans.p) {
			ss << "]";
		}

		if (is_imm) {
			//, #imm
			ss << utils::sformat(", #%s%" PRIu8, hw_data_trans.u ? "" : "-",
			                     hw_data_trans.imm);
		} else {
			//, reg
			ss << REGISTER_TABLE[(int)hw_data_trans.rm];
		}

		if (hw_data_trans.p) {
			ss << "]";

			if (hw_data_trans.w) {
				ss << "!";
			}
		}

		break;
	}

	case InstructionGroup::BLOCK_DATA_TRANSFER: {
		const auto &blk_data_trans = std::get<BlockDataTransfer>(group);

		if (blk_data_trans.rn == Register::SP && blk_data_trans.w) {
			ss << utils::sformat("%s\t{", (blk_data_trans.ld ? "pop" : "push"));
		} else {
			// ldm/stm rn(!), {}
			ss << utils::sformat("%s\t%s%s, {",
			                     blk_data_trans.ld ? "ldm" : "stm",
			                     REGISTER_TABLE[(int)blk_data_trans.rn],
			                     blk_data_trans.w ? "!" : "");
		}

		bool first = true;
		for (int i = 0; i < 16; ++i) {
			if (blk_data_trans.reg_list & (1 << i)) {
				if (!first)
					ss << ", ";

				ss << REGISTER_TABLE[i];
				first = false;
			}
		}

		ss << "}";
		break;
	}

	case InstructionGroup::DATA_SWAP: {
		const auto &data_swap = std::get<DataSwap>(group);

		// swpb/swp rd, rm, [rn]
		ss << utils::sformat("%s\t%s, %s, [%s]", data_swap.b ? "swpb " : "swp ",
		                     REGISTER_TABLE[(int)data_swap.rd],
		                     REGISTER_TABLE[(int)data_swap.rm],
		                     REGISTER_TABLE[(int)data_swap.rn]);
		break;
	}

	case InstructionGroup::BRANCH: {
		const auto &branch = std::get<Branch>(group);

		// b #imm
		ss << utils::sformat("%s\t#%" PRId32, (branch.link ? "bl " : "b "),
		                     branch.offset);
		break;
	}

	case InstructionGroup::BRANCH_EXCHANGE: {
		const auto &branchex = std::get<BranchEx>(group);

		// b rm
		ss << utils::sformat("bx\t%s", REGISTER_TABLE[(int)branchex.rm]);
		break;
	}

	case InstructionGroup::SWI: {
		ss << "swi";
		break;
	}

	case InstructionGroup::INVALID: {
		ss << "<INVALID>";
		break;
	}
	}

	return ss.str();
}

} // namespace charm::isa::arm
