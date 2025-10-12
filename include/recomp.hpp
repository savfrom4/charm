#pragma once
#include "arm.hpp"
#include "template.hpp"
#include <elfio/elfio.hpp>
#include <string>
#include <unordered_map>

// to automatically exclude comments
#define MINIFY_COMMENT(x) (_minify ? "" : x)
#define MINIFY_COMMENT_COMMA(x) (_minify ? "," : x)

namespace charm::recomp {

struct Function {
  std::string name;
  arm::addr_t address;
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

  void emit_meson_options(const std::string &output_dir);
  void emit_meson_project(const std::string &output_dir);

  /* Code level */

  void emit_data_header(const std::string &output_dir);
  void emit_data_source(const std::string &output_dir);
  void emit_code_source(const std::string &output_dir);
  void emit_code_header(const std::string &output_dir);

  /* Section level */

  void emit_code_address_mappings(Template &tl);
  void emit_code_stubs(Template &tl);
  void emit_code_section(std::ostream &os, const ELFIO::section *section);

  /* Instruction level */

  void emit_arm(std::ostream &os, const arm::Instruction &instr,
                arm::addr_t address);
  void emit_arm_data_processing(std::ostream &os, const arm::Instruction &instr,
                                arm::addr_t address);
  void emit_arm_multiply(std::ostream &os, const arm::Instruction &instr,
                         arm::addr_t address);
  void emit_arm_multiply_long(std::ostream &os, const arm::Instruction &instr,
                              arm::addr_t address);
  void emit_arm_branch(std::ostream &os, const arm::Instruction &instr,
                       arm::addr_t address);
  void emit_arm_data_transfer(std::ostream &os, const arm::Instruction &instr,
                              arm::addr_t address);
  void emit_arm_halfword_data_transfer(std::ostream &os,
                                       const arm::Instruction &instr,
                                       arm::addr_t address);

  void emit_arm_block_data_transfer(std::ostream &os,
                                    const arm::Instruction &instr,
                                    arm::addr_t address);

  // checks if instruction modifies pc
  void emit_arm_modifies_pc(std::ostream &os, const arm::Instruction &instr,
                            arm::addr_t address);

  template <typename... Args>
  void emit_arm_invalid(std::ostream &os, const arm::Instruction &instr,
                        arm::addr_t address, const char *fmt, Args... args) {
    char buffer[512] = {0};
    snprintf(buffer, 512, fmt, args...);

    os << std::hex << "throw std::runtime_error(\"" << buffer << " (addr = 0x"
       << address << ", raw=0x" << instr.value << ")\")";
  }

  /* Utils */

  std::string symbol_name_map(const std::string &symbol);
  bool section_is_data(const ELFIO::section *section);
  bool section_is_code(const ELFIO::section *section);

  bool _minify;
  ELFIO::elfio _elf;
  ELFIO::section *_text, *_relplt, *_reldyn, *_dynsym;

  std::vector<std::tuple<arm::addr_t, arm::addr_t>> _got_mappings;
  std::unordered_map<arm::addr_t, Function> _funs_reloc;
  std::unordered_map<arm::addr_t, Function> _funs_exports;
};

} // namespace charm::recomp
