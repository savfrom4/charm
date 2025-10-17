// PS: RIP clangd, i wish i didnt have to do .inls in year 2025...
// (I have to use constexpr in order for instruction decoding to be done at
// comptime)
#pragma once

namespace charm::isa::arm {

template <Word n, Word w = 1> inline constexpr Word get_bits(Word x) {
	return ((x >> n) & ((1U << w) - 1));
}

template <Word bits> inline constexpr std::int32_t sign_extend(Word value) {
	if (value & (1 << (bits - 1))) {
		return value | (~0u << bits);
	} else {
		return value;
	}
}

// 4.5.2 Shifts
inline constexpr Shifter Shifter::decode(Word value) {
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
inline constexpr DataProcessing DataProcessing::decode(Instruction &instr,
                                                       Word value) {
	instr.is_imm = get_bits<25>(value);     /* Immediate, bit 25 */
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
	if (instr.is_imm) {
		Word amount =
		    get_bits<8, 4>(value);        /* Amount to rotate by, bits 8-11 */
		Word imm = get_bits<0, 8>(value); /* Value, bits 0-7 */

		amount *= 2;

		if (!amount) {
			data.op2_imm = imm;
			return data;
		}

		data.op2_imm = (imm >> amount) | (imm << (32 - amount));
	} else {
		data.op2_reg = Shifter::decode(value);
	}

	return data;
}

// 4.7 Multiply and Multiply-Accumulate (MUL, MLA)
inline constexpr Multiply Multiply::decode(Instruction &instr, Word value) {
	instr.set_cflags = get_bits<20>(value); /* Set condition flags, bit 20 */

	return {
	    .a = (bool)get_bits<21>(value), /* Accumulate, bit 21 */
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
inline constexpr MultiplyLong MultiplyLong::decode(Instruction &instr,
                                                   Word value) {
	instr.set_cflags = get_bits<20>(value); /* Set condition flags, bit 20 */

	return {
	    .sign = (bool)get_bits<22>(value), /* Unsigned, bit 22 */
	    .a = (bool)get_bits<21>(value),    /* Accumulate, bit 21 */
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
inline constexpr DataTransfer DataTransfer::decode(Instruction &instr,
                                                   Word value) {
	instr.is_imm = !get_bits<25>(value); /* Immediate, bit 25 */

	DataTransfer data_trans = {
	    .p = (bool)get_bits<24>(value),  /* Pre/Post indexing, bit 24 */
	    .u = (bool)get_bits<23>(value),  /* Up/Down, bit 23 */
	    .b = (bool)get_bits<22>(value),  /* Byte/Word, bit 22 */
	    .w = (bool)get_bits<21>(value),  /* Writeback, bit 21 */
	    .ld = (bool)get_bits<20>(value), /* Load/Store, bit 20 */
	    .rn = static_cast<Register>(
	        get_bits<16, 4>(value)), /* Rn base register, bits 16-19 */
	    .rd = static_cast<Register>(
	        get_bits<12, 4>(value)), /* Rd src/dst register, bits 12-15 */
	};

	if (instr.is_imm) {
		data_trans.imm = get_bits<0, 12>(value);
	} else {
		data_trans.reg = Shifter::decode(value);
	}

	return data_trans;
}

// 4.10 Halfword and Signed Data Transfer
inline constexpr HalfWordDataTransfer
HalfWordDataTransfer::decode(Instruction &instr, Word value, bool imm) {
	instr.is_imm = imm;

	HalfWordDataTransfer hw_data_trans = {
	    .p = (bool)get_bits<24>(value),  /* Pre/Post indexing, bit 24 */
	    .u = (bool)get_bits<23>(value),  /* Up/Down, bit 23 */
	    .w = (bool)get_bits<21>(value),  /* Writeback, bit 21 */
	    .ld = (bool)get_bits<20>(value), /* Load/Store, bit 20 */
	    .rn =
	        static_cast<Register>(get_bits<16, 4>(value)), /* Rn, bits 16-19 */
	    .rd =
	        static_cast<Register>(get_bits<12, 4>(value)), /* Rd, bits 12-15 */
	    .type = static_cast<decltype(hw_data_trans.type)>(
	        get_bits<5, 2>(value)), /* Type, bits 5-6 */
	};

	if (imm) {
		Halfword offt_low =
		    get_bits<0, 4>(value); /* Imm offset Low, bits 0-3 */
		Halfword offt_high =
		    get_bits<8, 4>(value); /* Imm offset High bits 8-11 */

		hw_data_trans.imm = (Byte)((offt_high << 4) | offt_low);
	} else {
		hw_data_trans.rm =
		    static_cast<Register>(get_bits<0, 4>(value)); /* Rm, bits 0-3 */
	}

	return hw_data_trans;
}

// 4.11 Block Data Transfer (LDM, STM)
inline constexpr BlockDataTransfer BlockDataTransfer::decode(Word value) {
	return {
	    .p = (bool)get_bits<24>(value),   /* Pre/Post indexing, bit 24 */
	    .u = (bool)get_bits<23>(value),   /* Up/Down, bit 23 */
	    .psr = (bool)get_bits<22>(value), /* PSR & Force user, bit 22 */
	    .w = (bool)get_bits<21>(value),   /* Writeback, bit 21 */
	    .ld = (bool)get_bits<20>(value),  /* Load/Store, bit 20 */
	    .rn =
	        static_cast<Register>(get_bits<16, 4>(value)), /* Rn, bits 16-19 */
	    .reg_list =
	        (Halfword)get_bits<0, 16>(value), /* Register list, bits 0-15 */
	};
}

// 4.12 Single Data Swap (SWP)
inline constexpr DataSwap DataSwap::decode(Word value) {
	return {
	    .b = (bool)get_bits<22>(value), /* Byte/Word, bit 22  */
	    .rn = static_cast<Register>(
	        get_bits<16, 4>(value)), /* Rn base register, bits 16-19 */
	    .rd = static_cast<Register>(
	        get_bits<12, 4>(value)), /* Rd dst register, bits 12-15 */
	    .rm = static_cast<Register>(
	        get_bits<0, 4>(value)), /* Rm src register, bits 0-3 */
	};
}

// 4.4 Branch and Branch with Link (B, BL)
inline constexpr Branch Branch::decode(Word value) {
	return {
	    .link = (bool)get_bits<24>(value), /* Link, bit 24 */
	    .offset = sign_extend<26>((get_bits<0, 24>(value))
	                              << 2), /* Offset, bits 0-23 */
	};
}

// 4.3 Branch and Exchange (BX)
inline constexpr BranchEx BranchEx::decode(Word value) {
	return {
	    .rm = static_cast<Register>(get_bits<0, 4>(value)), /* Rn, bit 24 */
	};
}

// 4.13 Software Interrupt (SWI)
inline constexpr SWI SWI::decode(Word value) { return {}; }

inline constexpr Instruction::Instruction(Word value)
    : condition((Condition)get_bits<28, 4>(value)), value(value) {
	const auto bit_25 = get_bits<25>(value);

	switch (get_bits<26, 2>(value)) {

	// maybe data transfer
	case 0b00: {
		const auto bits_8_4 = get_bits<4, 4>(value);

		if (bits_8_4 == 0b1001) {
			Word type = get_bits<23, 5>(value);

			if (!type) {
				group = InstructionGroup::MULTIPLY;
				mul = Multiply::decode(*this, value);
			} else if (type == 0b00001) {
				group = InstructionGroup::MULTIPLY_LONG;
				mull = MultiplyLong::decode(*this, value);
			} else if (type == 0b00010 && !get_bits<8, 4>(value)) {
				group = InstructionGroup::DATA_SWAP;
				data_swap = DataSwap::decode(value);
			}
		}

		// check for specific branchex bit pattern
		else if (bits_8_4 == 0b0001 &&
		         get_bits<4, 22>(value) == 0b0100101111111111110001) {
			group = InstructionGroup::BRANCH_EXCHANGE;
			branchex = BranchEx::decode(value);
		}

		else if (!bit_25 && get_bits<7>(value) && get_bits<4>(value)) {
			if (get_bits<22>(value)) { // immediate
				group = InstructionGroup::HALFWORD_DATA_TRANSFER;
				hw_data_trans =
				    HalfWordDataTransfer::decode(*this, value, true);
			} else if (!get_bits<8, 4>(value)) { // register
				group = InstructionGroup::HALFWORD_DATA_TRANSFER;
				hw_data_trans =
				    HalfWordDataTransfer::decode(*this, value, false);
			}
		}

		else {
			group = InstructionGroup::DATA_PROCESSING;
			data = DataProcessing::decode(*this, value);
		}

		break;
	}

	// single data transfer
	case 0b01: {
		group = InstructionGroup::DATA_TRANSFER;
		data_trans = DataTransfer::decode(*this, value);
		break;
	}

	// branch / block data transfer
	case 0b10: {
		if (!bit_25) {
			group = InstructionGroup::BLOCK_DATA_TRANSFER;
			blk_data_trans = BlockDataTransfer::decode(value);
		} else { // branch
			group = InstructionGroup::BRANCH;
			branch = Branch::decode(value);
		}

		break;
	}

	// SWI
	case 0b11: {
		// those bits must be set for it to be SWI
		if (get_bits<24, 2>(value) != 0b11) {
			break;
		}

		group = InstructionGroup::SWI;
		swi = SWI::decode(value);
		break;
	}
	}
}

} // namespace charm::isa::arm
