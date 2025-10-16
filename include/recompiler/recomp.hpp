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
	void _step_analyze();
	void _step_emit(const std::string &output_dir);

	void _analyze_reloc_plt();
	void _analyze_reloc_dyn();
	void _analyze_exported_functions();

	void _emit_setup_project(const std::filesystem::path &output_dir);

	/* Code level */

	void _emit_data_header(const std::filesystem::path &output_dir);
	void _emit_data_source(const std::filesystem::path &output_dir);
	void _emit_code_source(const std::filesystem::path &output_dir);
	void _emit_code_header(const std::filesystem::path &output_dir);

	/* Section level */

	void _emit_code_address_mappings(Template &tl);
	void _emit_code_stubs(Template &tl);
	void _emit_code_section(std::ostream &os, const ELFIO::section *section);

	/* Instruction level */

	template <typename Array>
	inline std::string _emit_arm_from_table(Array array, int index,
	                                        Word value) {
		return utils::sformat("%s(0x%X);", array[index], value);
	}

	template <typename... Args>
	inline std::string _emit_arm_invalid(Word address, Word value,
	                                     const std::string &fmt, Args... args) {
		return utils::sformat(
		    "throw std::runtime_error(\"%s (addr=0x%X, raw=0x%X)\");",
		    utils::sformat(fmt, args...), address, value);
	}

	void _emit_arm(std::ostream &os, const isa::arm::Instruction &instr,
	               Word address);

	void _emit_arm_modifies_pc(std::ostream &os,
	                           const isa::arm::Instruction &instr,
	                           Word address);

	/* Utils */

	std::string _symbol_name_map(const std::string &symbol);
	bool _section_is_data(const ELFIO::section *section);
	bool _section_is_code(const ELFIO::section *section);

	bool _minify;
	ELFIO::elfio _elf;
	ELFIO::section *_text, *_relplt, *_reldyn, *_dynsym;

	std::vector<std::tuple<Word, Word>> _got_mappings;
	std::unordered_map<Word, Function> _funs_reloc;
	std::unordered_map<Word, Function> _funs_exports;
};

} // namespace charm::recomp
