#pragma once
#include <elfio/elfio.hpp>
#include <filesystem>
#include <string>
#include <unordered_map>

#include <arch.hpp>
#include <isa/arm.hpp>
#include <recompiler/template.hpp>

namespace charm::recomp {

struct Function {
	std::string name;
	Word address;
	bool is_external;
};

class Recompiler {
  public:
	Recompiler(const std::string &elf_exe, bool minify = false);

	void emit(const std::string &output_dir);

  private:
	void step_analyze();
	void step_emit(const std::string &output_dir);

	void analyze_reloc_plt();
	void analyze_reloc_dyn();
	void analyze_exported_functions();

	void emit_setup_project(const std::filesystem::path &output_dir);

	/* Code level */

	void emit_data_header(const std::filesystem::path &output_dir);
	void emit_data_source(const std::filesystem::path &output_dir);
	void emit_code_source(const std::filesystem::path &output_dir);
	void emit_code_header(const std::filesystem::path &output_dir);

	/* Section level */

	void emit_code_address_mappings(Template &tl);
	void emit_code_stubs(Template &tl);
	void emit_code_section(std::ostream &os, const ELFIO::section *section);

	/* Instruction level */

	void emit_arm(std::ostream &os, const isa::arm::Instruction &instr,
	              Word address);

	// checks if instruction modifies pc
	void emit_arm_modifies_pc(std::ostream &os,
	                          const isa::arm::Instruction &instr, Word address);

	template <typename... Args>
	void emit_arm_invalid(std::ostream &os, const isa::arm::Instruction &&instr,
	                      Word address, const char *fmt, Args... args) {
		char buffer[512] = {0};
		snprintf(buffer, 512, fmt, args...);

		os << std::hex << "throw std::runtime_error(\"" << buffer
		   << " (addr = 0x" << address << ", raw=0x" << instr.value << ")\")";
	}

	/* Utils */

	std::string symbol_name_map(const std::string &symbol);
	bool section_is_data(const ELFIO::section *section);
	bool section_is_code(const ELFIO::section *section);

	bool _minify;
	ELFIO::elfio _elf;
	ELFIO::section *_text, *_relplt, *_reldyn, *_dynsym;

	std::vector<std::tuple<Word, Word>> _got_mappings;
	std::unordered_map<Word, Function> _funs_reloc;
	std::unordered_map<Word, Function> _funs_exports;
};

} // namespace charm::recomp
