#pragma once
#include "libcharm/arm.hpp"
#include <cstdint>
#include <elfio/elfio.hpp>
#include <string>
#include <unordered_map>

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
  void analyze_map_plt_to_reloc();

  void emit_makefile(const std::string &output_dir);
  void emit_code_source(const std::string &output_dir);
  void emit_code_header(const std::string &output_dir);
  void emit_data_header(const std::string &output_dir);
  void emit_data_source(const std::string &output_dir);

  void emit_code_address_mappings(std::ofstream &ofs);
  void emit_code_stubs(std::ofstream &ofs);
  void emit_code_section(std::ofstream &ofs, const ELFIO::section *section);
  void emit_code_arm(std::ostream &os, const arm::Instruction &instr,
                     arm::addr_t address);

  template <typename... Args>
  void emit_code_invalid(std::ostream &os, const arm::Instruction &instr,
                         arm::addr_t address, const char *fmt, Args... args) {
    char buffer[512] = {0};
    snprintf(buffer, 512, fmt, args...);

    os << std::hex << "throw std::runtime_error(\"" << buffer << " (addr = 0x"
       << address << ", raw=0x" << instr.raw << ")\")";
  }

  bool _minify;
  ELFIO::elfio _elf;
  ELFIO::section *_text, *_plt, *_relplt, *_reldyn, *_dynsym;

  std::vector<std::tuple<arm::addr_t, arm::addr_t>> _got_mappings;
  std::unordered_map<arm::addr_t, Function> _funs_deps;
  std::unordered_map<arm::addr_t, Function> _funs_exports;
  std::unordered_map<arm::addr_t, Function *> _fun_deps_mapped;
};

} // namespace charm::recomp
