#include "libcharm/arm.hpp"
#include "libcharm/recomp.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace charm::recomp {

void Recompiler::step_emit(const std::string &output_dir) {
  const auto liblayer_path =
      std::filesystem::current_path() / "deps" / "liblayer";
  const auto symlink_path = std::filesystem::path{output_dir} / "liblayer";

  if (!std::filesystem::exists(liblayer_path)) {
    throw std::runtime_error(
        "Copy or symlink the \"liblayer\" directory from source "
        "to \"deps\" directory in current working direction.");
  }

  std::filesystem::create_directory(output_dir);

  if (!std::filesystem::exists(symlink_path)) {
    std::filesystem::create_symlink(liblayer_path, symlink_path);
  }

  emit_meson_options(output_dir);
  emit_meson_project(output_dir);

  std::cout << "> Code ..." << std::endl;
  emit_code_header(output_dir);
  emit_code_source(output_dir);

  std::cout << "> Data ..." << std::endl;
  emit_data_header(output_dir);
  emit_data_source(output_dir);
}

void Recompiler::emit_meson_options(const std::string &output_dir) {
  auto meson_options_path = std::filesystem::path{
      std::filesystem::path{output_dir} / "meson_options.txt"};

  if (std::filesystem::exists(meson_options_path)) {
    return;
  }

  std::ofstream ofs{meson_options_path};
  ofs << "option('debugging', type: 'boolean', value: false, "
         "description: "
         "'Enable debugging via charm-dbg')"
      << std::endl;
  ofs << "option('library', type: 'boolean', value: true, description: "
         "'Build as library')"
      << std::endl;
}

void Recompiler::emit_meson_project(const std::string &output_dir) {
  auto meson_project_path =
      std::filesystem::path{std::filesystem::path{output_dir} / "meson.build"};

  if (std::filesystem::exists(meson_project_path)) {
    return;
  }

  std::ofstream ofs{meson_project_path};
  ofs << "project('output', 'cpp')" << std::endl << std::endl;
  ofs << "subdir('liblayer')" << std::endl << std::endl;

  ofs << "sources = files('code.cpp', 'data.cpp')" << std::endl << std::endl;

  ofs << "if get_option('debugging')" << std::endl;
  ofs << "\tadd_project_arguments('-DLAYER_DEBUG', language : 'cpp')"
      << std::endl;
  ofs << "endif" << std::endl;

  ofs << "add_project_arguments('-Wno-unused-label', language : 'cpp')"
      << std::endl
      << std::endl;

  ofs << "if get_option('library')" << std::endl;
  ofs << "\toutput_dep = library('output', include_directories: "
         "[ '.' ], sources: "
         "sources, dependencies: [liblayer_dep])"
      << std::endl;
  ofs << "else" << std::endl;
  ofs << "\toutput_dep = executable('output', include_directories: "
         "[ '.' ], sources: "
         "sources, dependencies: [liblayer_dep])"
      << std::endl;
  ofs << "endif" << std::endl;
}

void Recompiler::emit_code_header(const std::string &output_dir) {
  std::ofstream ofs{
      std::filesystem::path{std::filesystem::path{output_dir} / "code.hpp"},
  };

  ofs << "#pragma once" << std::endl;
  ofs << "#include <liblayer/liblayer.hpp>" << std::endl;
  ofs << "#define INSTR_RETURN_LR (0xFFFFFFFF)" << std::endl << std::endl;

  ofs << "class ProgramState : public layer::ExecutionState {" << std::endl;
  ofs << "public:" << std::endl;
  ofs << "\tstd::uint32_t address_map(std::uintptr_t addr) override;"
      << std::endl;
  ofs << "\tstd::uintptr_t address_resolve(std::uint32_t addr) override;"
      << std::endl;
  ofs << "};" << std::endl << std::endl;

  ofs << "void eval(ProgramState& ps, std::uint32_t address);" << std::endl
      << std::endl;

  ofs << MINIFY_COMMENT("/* EXPORTED FUNCTIONS */") << std::endl << std::endl;

  for (auto &functions : _funs_exports) {
    ofs << "void export_" << symbol_name_map(functions.second.name)
        << "(ProgramState& ps);" << std::endl;
  }

  ofs << std::endl
      << MINIFY_COMMENT("/* EXTERNAL DEPENDENCIES */") << std::endl
      << std::endl;

  for (auto &functions : _funs_reloc) {
    if (!functions.second.is_external) {
      continue;
    }

    ofs << "void external_" << symbol_name_map(functions.second.name)
        << "(ProgramState& ps);" << std::endl;
  }

  ofs << std::endl;
}

void Recompiler::emit_code_source(const std::string &output_dir) {
  std::ofstream ofs{
      std::filesystem::path{std::filesystem::path{output_dir} / "code.cpp"},
  };

  ofs << "#define LAYER_IMPLEMENTATION" << std::endl;
  ofs << "#include <iostream>" << std::endl;
  ofs << "#include <stdexcept>" << std::endl;
  ofs << "#include <string>" << std::endl;
  ofs << "#include <liblayer/liblayer.hpp>" << std::endl;
  ofs << "#include \"code.hpp\"" << std::endl;
  ofs << "#include \"data.hpp\"" << std::endl << std::endl;
  ofs << "#define INSTR(ADDR) case ADDR: a##ADDR: ps.r[PC] = ADDR+8;"
      << std::endl;
  ofs << "#define JUMP(ADDR) address = ADDR;  goto __start__;" << std::endl;
  ofs << "#define EXPORT(name, address) __attribute__((weak)) void "
         "name (ProgramState& ps) { LAYER_DBE_SKIP(ps, \"%s\", \"external "
         "call: \" #name); "
         "ps.r[LR] = INSTR_RETURN_LR; "
         "eval(ps, address); }"
      << std::endl;
  ofs << "#define STUB(name) __attribute__((weak)) void "
         "name (ProgramState& ps) { LAYER_DBE_LOG(ps, \"%s\", \"unimplemented "
         "stub: \" #name); }"
      << std::endl;
  ofs << "using namespace layer;" << std::endl;

  ofs << std::endl
      << MINIFY_COMMENT("/* ADDRESS MAPPING */") << std::endl
      << std::endl;

  emit_code_address_mappings(ofs);
  emit_code_stubs(ofs);

  ofs << "void eval(ProgramState& ps, std::uint32_t address) {" << std::endl;
  ofs << "__start__:" << std::endl;
  ofs << "\tswitch(address) {" << std::endl;

  ofs << MINIFY_COMMENT(
             "\t// this is a special address that is used to return out of "
             "function when PC is set it.")
      << std::endl;

  ofs << "\tINSTR(INSTR_RETURN_LR) {" << std::endl;
  ofs << "\t\treturn;" << std::endl;
  ofs << "\t}" << std::endl << std::endl;

  ofs << MINIFY_COMMENT(
             "\t// mapping external functions to their .got addresses")
      << std::endl;

  ofs << std::hex;
  for (auto &functions : _funs_reloc) {
    if (!functions.second.is_external) {
      continue;
    }

    ofs << "\tINSTR(0x" << functions.second.address << ") {" << std::endl;
    ofs << "\t\texternal_" << symbol_name_map(functions.second.name) << "(ps);"
        << std::endl;
    ofs << "\t\taddress = ps.r[LR]; goto __start__;" << std::endl;
    ofs << "\t}" << std::endl << std::endl;
  }
  ofs << std::dec;

  for (auto &section : _elf.sections) {
    if (!section_is_code(section.get())) {
      continue;
    }

    emit_code_section(ofs, section.get());
  }

  ofs << "\tdefault:" << std::endl;

  if (_minify) {
    ofs << "\t\t__builtin_unreachable();";
  } else {
    ofs << "\t\tthrow std::runtime_error(\"Invalid address: \" + "
           "std::to_string(address));";
  }

  ofs << std::endl << "\t}" << std::endl << "}" << std::endl;
}

void Recompiler::emit_data_header(const std::string &output_dir) {
  std::ofstream ofs{
      std::filesystem::path{std::filesystem::path{output_dir} / "data.hpp"},
  };

  ofs << "/* THIS FILE IS AUTO-GENERATED BY charm STATIC "
         "RECOMPILER! DO NOT "
         "MODIFY DIRECTLY! */"
      << std::endl;

  ofs << "#pragma once" << std::endl;
  ofs << "#include <array>" << std::endl;
  ofs << "#include <cstdint>" << std::endl << std::endl;

  for (auto &section : _elf.sections) {
    if (!section_is_data(section.get())) {
      continue;
    }

    auto name = symbol_name_map(section->get_name());
    std::transform(name.begin(), name.end(), name.begin(), ::toupper);

    auto data_type = "std::uint8_t";
    auto data_size = section->get_size();

    // for .got entries, we actually store words
    if (section->get_name().find(".got") != std::string::npos) {
      data_type = "std::uint32_t";
      data_size /= sizeof(uint32_t);
    }

    ofs << ((section->get_flags() & ELFIO::SHF_WRITE) ? "extern"
                                                      : "extern const")
        << " std::array<" << data_type << ", " << data_size << "> g_" << name
        << "_DATA;" << std::endl;

    ofs << "inline constexpr std::uint32_t " << name << "_ADDR = 0x" << std::hex
        << section->get_address() << std::dec << "; /* Virtual address of "
        << section->get_name() << " */" << std::endl
        << std::endl;
  }
}

void Recompiler::emit_data_source(const std::string &output_dir) {
  std::ofstream ofs{
      std::filesystem::path{std::filesystem::path{output_dir} / "data.cpp"},
  };

  ofs << "/* THIS FILE IS AUTO-GENERATED BY charm STATIC "
         "RECOMPILER! DO NOT "
         "MODIFY DIRECTLY! */"
      << std::endl;

  ofs << "#include \"data.hpp\"" << std::endl << std::endl;

  for (auto &section : _elf.sections) {
    if (!section_is_data(section.get())) {
      continue;
    }

    const std::uint8_t *data =
        reinterpret_cast<const std::uint8_t *>(section->get_data());

    auto name = symbol_name_map(section->get_name());
    std::transform(name.begin(), name.end(), name.begin(), ::toupper);

    std::stringstream ss;

    // for non-got table we just write raw bytes or 0es
    if (section->get_name().find(".got") == std::string::npos) {
      ofs << ((section->get_flags() & ELFIO::SHF_WRITE)
                  ? "std::array<std::uint8_t, "
                  : "const std::array<std::uint8_t, ")
          << section->get_size() << "> g_" << name << "_DATA = {" << std::endl;

      ss << "\t";
      for (charm::arm::addr_t i = 0; i < section->get_size(); i++) {
        ss << (data ? static_cast<int>(data[i]) : 0) << ", ";

        if (i % 8 == 7) {
          ss << std::endl;
          ss << "\t";
        }
      }
    } else { // for got we map addresses that we know
      ofs << ((section->get_flags() & ELFIO::SHF_WRITE)
                  ? "std::array<std::uint32_t, "
                  : "const std::array<std::uint32_t, ")
          << section->get_size() / sizeof(std::uint32_t) << "> g_" << name
          << "_DATA = {" << std::endl;

      ss << std::hex;

      // map addresses
      for (arm::addr_t i = 0; i < section->get_size();
           i += sizeof(arm::instr_t)) {
        arm::addr_t mapped_address = 0;

        for (auto &mapping : _got_mappings) {
          arm::addr_t offset = std::get<0>(mapping) - section->get_address();
          if (offset != i) {
            continue;
          }

          mapped_address = std::get<1>(mapping);
          break;
        }

        ss << "\t0x" << mapped_address << "," << std::endl;
      }

      ss << std::dec;
    }

    ofs << ss.rdbuf() << std::endl << "};" << std::endl;
  }
}

void Recompiler::emit_code_address_mappings(std::ofstream &ofs) {
  ofs << "inline std::uint32_t ProgramState::address_map(std::uintptr_t addr) {"
      << std::endl;

  ofs << std::hex;

  ofs << "\tstd::uint32_t mapped;" << std::endl;
  ofs << "\tif((mapped = ExecutionState::address_map(addr))) { return mapped; }"
      << std::endl
      << std::endl;

  for (auto &section : _elf.sections) {
    if (!section_is_data(section.get())) {
      continue;
    }

    auto name = symbol_name_map(section->get_name());
    std::transform(name.begin(), name.end(), name.begin(), ::toupper);

    ofs << "\tif(addr >= reinterpret_cast<std::uintptr_t>(g_" << name
        << "_DATA.data())"
        << " && addr < reinterpret_cast<std::uintptr_t>(g_" << name
        << "_DATA.data()) + sizeof(g_" << name << "_DATA)) {" << std::endl;

    ofs << "\t\treturn 0x" << (uint32_t)section->get_address()
        << " + static_cast<uint32_t>("
        << "addr - reinterpret_cast<uintptr_t>(g_" << name << "_DATA.data()));"
        << std::endl;

    ofs << "\t}" << std::endl;
  }

  ofs << std::endl;
  ofs << "\tLAYER_DBE_LOG(*this, \"Error: unable to map address: 0x%X!\", "
         "addr);"
      << std::endl;
  ofs << "\tLAYER_DBE_SEND_PAUSED(*this);" << std::endl;
  ofs << "\tthrow std::runtime_error(\"address_map: unable to map "
         "address!\");"
      << std::endl;

  ofs << "}" << std::endl << std::endl;

  ofs << "inline std::uintptr_t ProgramState::address_resolve(std::uint32_t "
         "addr) {"
      << std::endl;

  ofs << "\tstd::uintptr_t mapped;" << std::endl;
  ofs << "\tif((mapped = ExecutionState::address_resolve(addr))) { return "
         "mapped; }"
      << std::endl
      << std::endl;

  for (auto &section : _elf.sections) {
    if (!section_is_data(section.get())) {
      continue;
    }

    auto name = symbol_name_map(section->get_name());
    std::transform(name.begin(), name.end(), name.begin(), ::toupper);

    ofs << "\tif(addr >= 0x" << section->get_address() << " && addr < 0x"
        << section->get_address() + section->get_size() << ") {" << std::endl;

    ofs << "\t\treturn "
           "reinterpret_cast<std::uintptr_t>(&reinterpret_cast<const "
           "char*>(g_"
        << name << "_DATA.data())[addr - 0x" << section->get_address() << "]);"
        << std::endl;

    ofs << "\t}" << std::endl;
  }

  ofs << std::endl;
  ofs << "\tLAYER_DBE_LOG(*this, \"Error: unable to resolve address: 0x%X!\", "
         "addr);"
      << std::endl;
  ofs << "\tLAYER_DBE_SEND_PAUSED(*this);" << std::endl;
  ofs << "\tthrow std::runtime_error(\"address_resolve: unable to resolve "
         "address!\");"
      << std::endl;

  ofs << std::dec;
  ofs << "}" << std::endl << std::endl;
}

void Recompiler::emit_code_stubs(std::ofstream &ofs) {
  ofs << std::endl
      << MINIFY_COMMENT("/* EXPORTED FUNCTIONS */") << std::endl
      << std::endl;

  ofs << std::hex;
  for (auto &functions : _funs_exports) {
    ofs << "EXPORT(export_" << symbol_name_map(functions.second.name) << ", 0x"
        << functions.second.address << ");" << std::endl;
  }
  ofs << std::dec;

  ofs << std::endl
      << MINIFY_COMMENT("/* EXTERNAL DEPENDENCIES */") << std::endl
      << std::endl;

  for (auto &functions : _funs_reloc) {
    if (!functions.second.is_external) {
      continue;
    }

    ofs << "STUB(external_" << symbol_name_map(functions.second.name) << ");"
        << std::endl;
  }

  ofs << std::endl;
}

std::string Recompiler::symbol_name_map(const std::string &symbol) {
  std::string s;

  for (auto &ch : symbol) {
    if (isspace(ch)) {
      s += '_';
      continue;
    }

    if (!isalpha(ch) && !isdigit(ch) && ch != '_') {
      continue;
    }

    s += ch;
  }

  return s;
}

bool Recompiler::section_is_data(const ELFIO::section *section) {
  auto flags = section->get_flags();
  if (!(flags & ELFIO::SHF_ALLOC) && !(flags & ELFIO::SHF_EXECINSTR)) {
    return false;
  }

  auto type = section->get_type();
  if (type == ELFIO::SHT_SYMTAB || type == ELFIO::SHT_DYNSYM ||
      type == ELFIO::SHT_RELA) {
    return false;
  }

  if (section->get_name().find("padding") != std::string::npos) {
    return false;
  }

  if (!section->get_size()) {
    return false;
  }

  return true;
}

bool Recompiler::section_is_code(const ELFIO::section *section) {
  if (!(section->get_flags() & ELFIO::SHF_EXECINSTR)) {
    return false;
  }

  if (!section->get_data()) {
    return false;
  }

  return true;
}

} // namespace charm::recomp
