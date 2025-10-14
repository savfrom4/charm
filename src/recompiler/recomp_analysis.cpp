#include <ostream>
#include <sstream>
#include <tuple>

#include <recompiler/recomp.hpp>

namespace charm::recomp {

void Recompiler::step_analyze() {
	analyze_reloc_dyn();
	analyze_reloc_plt();

	analyze_exported_functions();
}

// This step iterates trough .GOT table in the ELF binary and collects
// symbols as well as their names. The entries are later used to build virtual
// .GOT mappings.
void Recompiler::analyze_reloc_dyn() {
	std::cout << "> Inspecting dyn relocation table ..." << std::endl;

	if (!_reldyn) {
		std::cout << "\treldyn is not present!" << std::endl;
		return;
	}

	ELFIO::symbol_section_accessor symbols(_elf, _dynsym);
	ELFIO::relocation_section_accessor relocations(_elf, _reldyn);

	for (ELFIO::Elf_Xword i = 0; i < relocations.get_entries_num(); i++) {
		ELFIO::Elf64_Addr offset;
		ELFIO::Elf_Word symbol_index;
		unsigned type;
		ELFIO::Elf_Sxword addend;

		if (!relocations.get_entry(i, offset, symbol_index, type, addend)) {
			continue;
		}

		std::string name;
		ELFIO::Elf64_Addr value;
		ELFIO::Elf_Xword size;
		unsigned char bind, type_sym, other;
		ELFIO::Elf_Half shndx;

		if (!symbols.get_symbol(symbol_index, name, value, size, bind, type_sym,
		                        shndx, other)) {
			continue;
		}

		// TODO: store name too ?
		_got_mappings.push_back(std::make_tuple(offset, value));
	}

	std::cout << "\tMapped " << _got_mappings.size() << " symbols ..."
	          << std::endl;
}

// In this step we iterate trough relplt (Relocation Table) in search for
// functions and their .GOT offset. We however don't care about the
// offset, rather what we need is function's virtual address. If its zero, that
// means function is expected to be mapped by the linker (external). But, if it
// has an address that most certanly means the function is included within the
// binary (internal).
void Recompiler::analyze_reloc_plt() {
	std::cout << "> Inspecting the relocation table ..." << std::endl;

	if (!_relplt) {
		std::cout << "\trelplt is not present!" << std::endl;
		return;
	}

	ELFIO::symbol_section_accessor symbols(_elf, _dynsym);
	ELFIO::relocation_section_accessor relocations(_elf, _relplt);

	for (ELFIO::Elf_Xword i = 0; i < relocations.get_entries_num(); i++) {
		ELFIO::Elf64_Addr offset;
		ELFIO::Elf_Word symbol_index;
		unsigned type;
		ELFIO::Elf_Sxword addend;

		if (!relocations.get_entry(i, offset, symbol_index, type, addend)) {
			continue;
		}

		std::string name;
		ELFIO::Elf64_Addr value;
		ELFIO::Elf_Xword size;
		unsigned char bind, type_sym, other;
		ELFIO::Elf_Half shndx;

		if (!symbols.get_symbol(symbol_index, name, value, size, bind, type_sym,
		                        shndx, other)) {
			std::stringstream ss;
			ss << "unknown_0x" << std::hex << value;

			// included within the binary
			std::cout << "\t" << std::hex << ss.str() << " offset 0x" << offset
			          << std::dec << std::endl;

			_funs_reloc[offset] = Function{
			    .name = ss.str(),
			    .address = (Word)offset,
			    .is_external = true,
			};
			continue;
		}

		// included within the binary
		std::cout << "\t" << std::hex << name << " offset 0x" << offset
		          << std::dec << std::endl;

		if (value != 0) {
			std::cout << "\t" << name << " is internal ..." << std::endl;
		}

		_funs_reloc[offset] = Function{
		    .name = name,
		    .address = (Word)(value != 0 ? value    // virtual address
		                                 : offset), // .got offset
		    .is_external = !value,
		};

		_got_mappings.push_back(
		    std::make_tuple(offset, (Word)(value != 0 ? value : offset)));
	}

	std::cout << "\tFound " << _funs_reloc.size() << " functions!" << std::endl;
}

// This step collects all functions that executable "exports".
void Recompiler::analyze_exported_functions() {
	std::cout << "> Inspecting exported functions ..." << std::endl;

	ELFIO::symbol_section_accessor symbols(_elf, _dynsym);

	for (ELFIO::Elf_Xword i = 0; i < symbols.get_symbols_num(); i++) {
		std::string name;
		ELFIO::Elf64_Addr value;
		ELFIO::Elf_Xword size;
		unsigned char bind, type_sym, other;
		ELFIO::Elf_Half shndx;

		if (!symbols.get_symbol(i, name, value, size, bind, type_sym, shndx,
		                        other)) {
			continue;
		}

		if ((bind != ELFIO::STB_GLOBAL && bind != ELFIO::STB_WEAK) ||
		    type_sym != ELFIO::STT_FUNC || shndx == ELFIO::SHN_UNDEF) {
			continue;
		}

		_funs_exports[value] = Function{
		    .name = name, // TODO: multiple entries can have the same name!
		    .address = (Word)value,
		    .is_external = false,
		};
	}

	std::cout << "\tFound " << _funs_exports.size() << " functions!"
	          << std::endl;
}
} // namespace charm::recomp
