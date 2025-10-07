#include "elfio/elfio.hpp"
#include <arm.hpp>
#include <recomp.hpp>

inline static const std::string VERSION = "0.2.0";
inline static const std::string RECOMP = "recomp";
inline static const std::string DUMP = "dump";
inline static const std::string MINIFY = "--minify";

void help_show();
void dump(const std::string &elf_exe, const std::string &dump_file);
void disassemble(std::ofstream &ofs, ELFIO::section *section);
void dump_symtable(std::ofstream &ofs, ELFIO::elfio &elf,
                   ELFIO::section *section);

int main(int argc, char **argv) {
	if (argc < 4) {
		help_show();
		return 1;
	}

	bool minify = false;
	for (int i = 0; i < argc; i++) {
		if (argv[i] == MINIFY) {
			minify = true;
			break;
		}
	}

	if (argv[1] == RECOMP) {
		charm::recomp::Recompiler recomp{argv[2], minify};
		recomp.emit(argv[3]);
	} else if (argv[1] == DUMP) {
		dump(argv[2], argv[3]);
	} else {
		help_show();
	}

	return 0;
}

void help_show() {
	std::cout << "charm-cli v" << VERSION
	          << " — A static ARM-to-C++ recompilation and disassembly tool."
	          << std::endl;
	std::cout
	    << "Licensed under the MIT License © 2025 sstochi and contributors."
	    << std::endl
	    << std::endl;

	std::cout << "Usage:\n"
	          << "\tcharm-cli <MODE> <elf_binary> <output>\n"
	          << std::endl;

	std::cout
	    << "Modes:\n"
	    << "\trecomp\tRecompile the executable into a C++ meson project.\n"
	    << "\tdump\tAnalyze the executable and dump instructions.\n"
	    << std::endl;

	std::cout << "Arguments:\n"
	          << "\t<elf_binary>\tPath to the ELF executable file.\n"
	          << "\t<output>\tOutput path:\n"
	          << "\t\t\t- For 'recomp', a directory to write project files.\n"
	          << "\t\t\t- For 'dump', a single file to write the output.\n"
	          << std::endl;

	std::cout
	    << "Optional Arguments:\n"
	    << "\t--minify\tMinimize the produced C++ code to reduce compilation "
	       "time. The output might be harder to read.\n"
	    << std::endl;

	std::cout
	    << "Examples:\n"
	    << "\tcharm-cli recomp libfoo.so build/ // standard elf executable "
	       "recompilation \n"
	    << "\tcharm-cli recomp libfmath.so out/ --minify // minified "
	       "recompilation \n"
	    << "\tcharm-cli dump libfoo.so dump.txt // dump instructions \n";
}

void dump(const std::string &elf_exe, const std::string &dump_file) {
	std::ofstream ofs{dump_file};

	ELFIO::elfio elf;
	elf.load(elf_exe);

	ELFIO::section *symtab = elf.sections[".symtab"];
	if (symtab && symtab->get_type() != ELFIO::SHT_SYMTAB) {
		dump_symtable(ofs, elf, symtab);
	}

	symtab = elf.sections[".dynsym"];
	if (symtab && symtab->get_type() != ELFIO::SHT_SYMTAB) {
		dump_symtable(ofs, elf, symtab);
	}

	ELFIO::section *text = elf.sections[".text"];
	if (!text) {
		throw std::runtime_error("No .text section found.");
	}

	ELFIO::section *plt = elf.sections[".plt"];
	if (!plt) {
		return;
	}

	disassemble(ofs, text);
	disassemble(ofs, plt);
}

void disassemble(std::ofstream &ofs, ELFIO::section *section) {
	ofs << "SECTION \"" << section->get_name() << "\" (addr 0x" << std::hex
	    << section->get_address() << std::dec << ", size "
	    << section->get_size() << "):" << std::endl;

	const char *data = section->get_data();
	size_t data_size = section->get_size();

	for (charm::arm::addr_t i = 0; i < data_size;
	     i += sizeof(charm::arm::instr_t)) {
		charm::arm::instr_t instr_raw;
		memcpy(&instr_raw, data + i, sizeof(charm::arm::instr_t));

		ofs << "\t0x" << std::hex << section->get_address() + i << ": "
		    << std::dec;
		ofs << charm::arm::Instruction::decode(instr_raw).dump() << std::endl;
	}

	ofs << std::endl;
}

void dump_symtable(std::ofstream &ofs, ELFIO::elfio &elf,
                   ELFIO::section *section) {
	ofs << "SECTION \"" << section->get_name() << "\" (addr 0x" << std::hex
	    << section->get_address() << std::dec << ", size "
	    << section->get_size() << "):" << std::endl;

	ELFIO::symbol_section_accessor symbols(elf, section);

	for (unsigned int i = 0; i < symbols.get_symbols_num(); i++) {
		std::string name;
		ELFIO::Elf64_Addr value;
		ELFIO::Elf_Xword size;
		unsigned char bind, type, other;
		ELFIO::Elf_Half section_idx;

		if (!symbols.get_symbol(i, name, value, size, bind, type, section_idx,
		                        other)) {
			continue;
		}

		if (type != ELFIO::STT_FUNC) {
			continue;
		}

		ofs << "\t0x" << std::hex << value << ": " << std::dec << name
		    << std::endl;
	}

	ofs << std::endl;
}
