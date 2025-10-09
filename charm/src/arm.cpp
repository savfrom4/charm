#include "arm.hpp"
#include "utils.hpp"
#include <array>
#include <inttypes.h>
#include <sstream>

const std::array<std::string, (int)charm::arm::Opcode::COUNT> OPCODE_TABLE = {
    "and", "eor", "sub", "rsb", "add", "adc", "sbc", "rsc",
    "tst", "teq", "cmp", "cmn", "orr", "mov", "bic", "mvn",
};

const std::array<std::string, (int)charm::arm::Register::COUNT> REGISTER_TABLE =
    {
        "r0", "r1", "r2",  "r3",  "r4", "r5", "r6", "r7",
        "r8", "r9", "r10", "r11", "ip", "sp", "lr", "pc",
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
Instruction Instruction::decode(instr_t value) {
	Instruction info;
	info.value = value;
	info.condition = static_cast<Condition>(get_bits<28, 4>(value));

	switch (get_bits<26, 2>(value)) {
	// maybe data transfer
	case 0b00: {
		switch (get_bits<4, 4>(value)) {

		// multiply / multiply long / single data swap
		case 0b1001: {
			std::uint32_t type = get_bits<23, 5>(value);

			if (!type) {
				info.group = Multiply::decode(info, value);
				return info;
			} else if (type == 0b00001) {
				info.group = MultiplyLong::decode(info, value);
				return info;
			} else if (type == 0b00010 && !get_bits<8, 4>(value)) {
				info.group = DataSwap::decode(value);
				return info;
			}

			break;
		}

		// branch exchange
		case 0b0001: {
			if (get_bits<4, 22>(value) == 0b0100101111111111110001) {
				info.group = BranchEx::decode(value);
				return info;
			}
			break;
		}
		}

		// half word transfer
		if (!get_bits<25>(value) && get_bits<7>(value) && get_bits<4>(value)) {
			if (get_bits<22>(value)) { // immediate
				info.group = HalfWordDataTransfer::decode(info, value, true);
				return info;
			} else if (!get_bits<8, 4>(value)) { // register
				info.group = HalfWordDataTransfer::decode(info, value, false);
				return info;
			}

			break;
		}

		info.group = DataProcessing::decode(info, value);
		break;
	}

	// single data transfer
	case 0b01: {
		info.group = DataTransfer::decode(info, value);
		break;
	}

	// branch / block data transfer
	case 0b10: {
		if (!get_bits<25>(value)) {
			info.group = BlockDataTransfer::decode(value);
		} else { // branch
			info.group = Branch::decode(value);
		}

		break;
	}

	// SWI
	case 0b11: {
		// those bits must be set for it to be SWI
		if (get_bits<24, 2>(value) != 0b11) {
			break;
		}

		info.group = SWI::decode(value);
		break;
	}
	}

	return info;
}

// 4.5.2 Shifts
Shifter Shifter::decode(instr_t value) {
	Shifter shift = {
	    .type = static_cast<decltype(shift.type)>(get_bits<5, 2>(value)),
	    .is_reg =
	        (bool)get_bits<4>(value), /* Shift register should be used, bit 4 */
	    .rm = static_cast<Register>(
	        get_bits<0, 4>(value)), /* Rm register, bits 0-3 */
	};

	if (shift.is_reg) {
		shift.amount_or_rs = get_bits<8, 4>(value); /* Rs register, bits 8-11 */
	} else {
		shift.amount_or_rs =
		    get_bits<7, 5>(value); /* Shift amount, bits 7-11 */
	}

	return shift;
}

// 4.5 Data Processing
DataProcessing DataProcessing::decode(Instruction &instr, instr_t value) {
	instr.immediate = get_bits<25>(value);  /* Immediate, bit 25 */
	instr.set_cflags = get_bits<20>(value); /* Set condition flags, bit 20 */

	DataProcessing data = {
	    .op = static_cast<Opcode>(
	        get_bits<21, 4>(value)), /* Opcode, bits 21-24 */
	    .rn = static_cast<Register>(
	        get_bits<16, 4>(value)), /* Rn register, bits 16-19 */
	    .rd = static_cast<Register>(
	        get_bits<12, 4>(value)), /* Rd register, bits 12-15 */
	};

	// Operand 2
	if (instr.immediate) {
		std::uint32_t rotate =
		    get_bits<8, 4>(value); /* Amount to rotate by, bits 8-11 */

		std::uint32_t imm = get_bits<0, 8>(value); /* Value, bits 0-7 */
		rotate *= 2;
		data.op2_imm = (imm >> rotate) | (imm << (32 - rotate));
	} else {
		data.op2_reg = Shifter::decode(value);
	}

	return data;
}

// 4.7 Multiply and Multiply-Accumulate (MUL, MLA)
Multiply Multiply::decode(Instruction &instr, instr_t value) {
	instr.set_cflags = get_bits<20>(value); /* Set condition flags, bit 20 */

	return {
	    .accumulate = (bool)get_bits<21>(value), /* Accumulate, bit 21 */
	    .rd = static_cast<Register>(
	        get_bits<16, 4>(value)), /* Rd register, bits 16-19 */
	    .rn = static_cast<Register>(
	        get_bits<12, 4>(value)), /* Rn register, bits 16-19 */
	    .rs = static_cast<Register>(
	        get_bits<8, 4>(value)), /* Rs register, bits 8-11 */
	    .rm = static_cast<Register>(
	        get_bits<0, 4>(value)), /* Rm register, bits 0-3 */
	};
}

// 4.8 Multiply Long and Multiply-Accumulate Long (MULL,MLAL)
MultiplyLong MultiplyLong::decode(Instruction &instr, instr_t value) {
	instr.set_cflags = get_bits<20>(value); /* Set condition flags, bit 20 */

	return {
	    .sign = (bool)get_bits<22>(value),       /* Unsigned, bit 22 */
	    .accumulate = (bool)get_bits<21>(value), /* Accumulate, bit 21 */
	    .rd_hi = static_cast<Register>(
	        get_bits<16, 4>(value)), /* RdHi register, bits 16-19 */
	    .rd_lo = static_cast<Register>(
	        get_bits<12, 4>(value)), /* RdLo register, bits 16-19 */
	    .rs = static_cast<Register>(
	        get_bits<8, 4>(value)), /* Rs register, bits 8-11 */
	    .rm = static_cast<Register>(
	        get_bits<0, 4>(value)), /* Rm register, bits 0-3 */
	};
}

// 4.9 Single Data Transfer (LDR, STR)
DataTransfer DataTransfer::decode(Instruction &instr, instr_t value) {
	instr.immediate = !get_bits<25>(value); /* Immediate, bit 25 */

	DataTransfer data_trans = {
	    .pre_indx = (bool)get_bits<24>(value),   /* Pre/Post indexing, bit 24 */
	    .add = (bool)get_bits<23>(value),        /* Up/Down, bit 23 */
	    .byte = (bool)get_bits<22>(value),       /* Byte/Word, bit 22 */
	    .write_back = (bool)get_bits<21>(value), /* Writeback, bit 21 */
	    .load = (bool)get_bits<20>(value),       /* Load/Store, bit 20 */
	    .rn = static_cast<Register>(
	        get_bits<16, 4>(value)), /* Rn base register, bits 16-19 */
	    .rd = static_cast<Register>(
	        get_bits<12, 4>(value)), /* Rd src/dst register, bits 12-15 */
	};

	if (instr.immediate) {
		data_trans.offset_imm = get_bits<0, 12>(value);
	} else {
		data_trans.offset_reg = Shifter::decode(value);
	}

	return data_trans;
}

// 4.10 Halfword and Signed Data Transfer
HalfWordDataTransfer HalfWordDataTransfer::decode(Instruction &instr,
                                                  instr_t value, bool imm) {
	instr.immediate = imm;

	HalfWordDataTransfer hw_data_trans = {
	    .pre_indx = (bool)get_bits<24>(value),   /* Pre/Post indexing, bit 24 */
	    .add = (bool)get_bits<23>(value),        /* Up/Down, bit 23 */
	    .write_back = (bool)get_bits<21>(value), /* Writeback, bit 21 */
	    .load = (bool)get_bits<20>(value),       /* Load/Store, bit 20 */
	    .rn =
	        static_cast<Register>(get_bits<16, 4>(value)), /* Rn, bits 16-19 */
	    .rd =
	        static_cast<Register>(get_bits<12, 4>(value)), /* Rd, bits 12-15 */
	    .type = static_cast<decltype(hw_data_trans.type)>(
	        get_bits<5, 2>(value)), /* Type, bits 5-6 */
	};

	if (imm) {
		uint8_t offt_low = static_cast<uint16_t>(
		    get_bits<0, 4>(value)); /* Imm offset Low, bits 0-3 */
		uint8_t offt_high = static_cast<uint16_t>(
		    get_bits<8, 4>(value)); /* Imm offset High bits 8-11 */

		hw_data_trans.offset_imm =
		    static_cast<uint8_t>((offt_high << 4) | offt_low);
	} else {
		hw_data_trans.offset_reg =
		    static_cast<Register>(get_bits<0, 4>(value)); /* Rm, bits 0-3 */
	}

	return hw_data_trans;
}

// 4.11 Block Data Transfer (LDM, STM)
BlockDataTransfer BlockDataTransfer::decode(instr_t value) {
	return {
	    .pre_indx = (bool)get_bits<24>(value),   /* Pre/Post indexing, bit 24 */
	    .add = (bool)get_bits<23>(value),        /* Up/Down, bit 23 */
	    .psr = (bool)get_bits<22>(value),        /* PSR & Force user, bit 22 */
	    .write_back = (bool)get_bits<21>(value), /* Writeback, bit 21 */
	    .load = (bool)get_bits<20>(value),       /* Load/Store, bit 20 */
	    .rn =
	        static_cast<Register>(get_bits<16, 4>(value)), /* Rn, bits 16-19 */
	    .reg_list = (std::uint16_t)get_bits<0, 16>(
	        value), /* Register list, bits 0-15 */
	};
}

// 4.12 Single Data Swap (SWP)
DataSwap DataSwap::decode(instr_t value) {
	return {
	    .byte = (bool)get_bits<22>(value), /* Byte/Word, bit 22  */
	    .rn = static_cast<Register>(
	        get_bits<16, 4>(value)), /* Rn base register, bits 16-19 */
	    .rd = static_cast<Register>(
	        get_bits<12, 4>(value)), /* Rd dst register, bits 12-15 */
	    .rm = static_cast<Register>(
	        get_bits<0, 4>(value)), /* Rm src register, bits 0-3 */
	};
}

// 4.4 Branch and Branch with Link (B, BL)
Branch Branch::decode(instr_t value) {
	return {
	    .link = (bool)get_bits<24>(value), /* Link, bit 24 */
	    .offset = sign_extend<26>((get_bits<0, 24>(value))
	                              << 2), /* Offset, bits 0-23 */
	};
}

// 4.3 Branch and Exchange (BX)
BranchEx BranchEx::decode(instr_t value) {
	return {
	    .rm = static_cast<Register>(get_bits<0, 4>(value)), /* Rn, bit 24 */
	};
}

// 4.13 Software Interrupt (SWI)
SWI SWI::decode(instr_t value) { return {}; }

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

		if (immediate) { // #imm
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
		ss << utils::sformat("%s\t%s, %s, %s", (mul.accumulate ? "mla" : "mul"),
		                     REGISTER_TABLE[(int)mul.rd],
		                     REGISTER_TABLE[(int)mul.rm],
		                     REGISTER_TABLE[(int)mul.rs]);

		// , rn
		if (mul.accumulate) {
			ss << ", " << REGISTER_TABLE[(int)mul.rn];
		}

		break;
	}

	case InstructionGroup::MULTIPLY_LONG: {
		const auto &mul_long = std::get<MultiplyLong>(group);

		// prefix
		ss << (mul_long.sign ? "s" : "u");

		// (s/u)mul/mlal rd_lo, rd_hi, rm, rs
		ss << utils::sformat(
		    "%s\t%s, %s, %s, %s", mul_long.accumulate ? "mlal" : "mull",
		    REGISTER_TABLE[(int)mul_long.rd_lo],
		    REGISTER_TABLE[(int)mul_long.rd_hi],
		    REGISTER_TABLE[(int)mul_long.rm], REGISTER_TABLE[(int)mul_long.rs]);
		break;
	}

	case InstructionGroup::DATA_TRANSFER: {
		const auto &data_trans = std::get<DataTransfer>(group);

		// push/pop rd
		if (immediate && data_trans.write_back &&
		    data_trans.rn == Register::SP && data_trans.offset_imm == 4 &&
		    data_trans.add == data_trans.load) {
			ss << utils::sformat("%s\t{%s}", data_trans.load ? "pop" : "push",
			                     REGISTER_TABLE[(int)data_trans.rd]);
			break;
		}

		// ldr/str(b) rd, [rn
		ss << utils::sformat("%s%s\t%s, [%s", data_trans.load ? "ldr" : "str",
		                     data_trans.byte ? "b" : "",
		                     REGISTER_TABLE[(int)data_trans.rd],
		                     REGISTER_TABLE[(int)data_trans.rn]);

		// close the square bracket if post indexed
		if (!data_trans.pre_indx) {
			ss << "]";
		}

		if (immediate) {
			//, #imm
			ss << utils::sformat(", #%s%" PRIu16, data_trans.add ? "" : "-",
			                     data_trans.offset_imm);
		} else {
			ss << REGISTER_TABLE[(int)data_trans.offset_reg.rm];

			// , shift
			if (data_trans.offset_reg.amount_or_rs != 0) {
				ss << utils::sformat(
				    ", %s%s", SHIFT_TABLE[(int)data_trans.offset_reg.type],
				    (data_trans.offset_reg.is_reg
				         ? utils::sformat(", %s",
				                          REGISTER_TABLE[data_trans.offset_reg
				                                             .amount_or_rs])
				         : utils::sformat(" #%" PRIu8,
				                          data_trans.offset_reg.amount_or_rs)));
			}
		}

		if (data_trans.pre_indx) {
			ss << "]";

			if (data_trans.write_back) {
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

		ss << utils::sformat("%s%s\t%s, [%s",
		                     hw_data_trans.load ? "ldr" : "str",
		                     type_table[(int)hw_data_trans.type],
		                     REGISTER_TABLE[(int)hw_data_trans.rd],
		                     REGISTER_TABLE[(int)hw_data_trans.rn]);

		// close the square bracket if post indexed
		if (!hw_data_trans.pre_indx) {
			ss << "]";
		}

		if (immediate) {
			//, #imm
			ss << utils::sformat(", #%s%" PRIu8, hw_data_trans.add ? "" : "-",
			                     hw_data_trans.offset_imm);
		} else {
			//, reg
			ss << REGISTER_TABLE[(int)hw_data_trans.offset_reg];
		}

		if (hw_data_trans.pre_indx) {
			ss << "]";

			if (hw_data_trans.write_back) {
				ss << "!";
			}
		}

		break;
	}

	case InstructionGroup::BLOCK_DATA_TRANSFER: {
		const auto &blk_data_trans = std::get<BlockDataTransfer>(group);

		if (blk_data_trans.rn == Register::SP && blk_data_trans.write_back) {
			ss << utils::sformat("%s\t{",
			                     (blk_data_trans.load ? "pop" : "push"));
		} else {
			// ldm/stm rn(!), {}
			ss << utils::sformat("%s\t%s%s, {",
			                     blk_data_trans.load ? "ldm" : "stm",
			                     REGISTER_TABLE[(int)blk_data_trans.rn],
			                     blk_data_trans.write_back ? "!" : "");
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
		ss << utils::sformat("%s\t%s, %s, [%s]",
		                     data_swap.byte ? "swpb " : "swp ",
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
	}

	return ss.str();
}

} // namespace charm::arm
