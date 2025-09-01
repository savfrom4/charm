#include "elfio/elfio.hpp"
#include <libcharm/arm.hpp>
#include <libcharm/emulator.hpp>
#include <libcharm/recomp.hpp>

const std::string VERSION = "0.01.00";
const std::string RECOMP = "recomp";
const std::string DUMP = "dump";
const std::string MINIFY = "--minify";

void show_help();
void dump(const std::string &elf_exe, const std::string &dump_file);
void dump_instructions(std::ofstream &ofs, ELFIO::section *section);
void dump_symtable(std::ofstream &ofs, ELFIO::elfio &elf,
                   ELFIO::section *section);

int main(int argc, char **argv) {
  if (argc < 4) {
    show_help();
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
    show_help();
  }

  return 0;
}

void show_help() {
  std::cout << "charm-cli v" << VERSION
            << " — A static ARM-to-C++ recompilation and analysis tool."
            << std::endl;
  std::cout << "Licensed under the MIT License © 2025 sstochi and contributors."
            << std::endl
            << std::endl;

  std::cout
      << "Usage:\n"
      << "\tcharm-cli [MODE] <elf_binary> <output> [function_address...]\n"
      << std::endl;

  std::cout << "Modes:\n"
            << "\trecomp\tRecompile the executable into C++ project.\n"
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

  std::cout << "Examples:\n"
            << "\tcharm-cli recomp libfmath.so out/ --minify\n"
            << "\tcharm-cli recomp libfoo.so build/\n"
            << "\tcharm-cli dump libfoo.so dump.txt\n";
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

  dump_instructions(ofs, text);
  dump_instructions(ofs, plt);
}

void dump_instructions(std::ofstream &ofs, ELFIO::section *section) {
  ofs << "SECTION \"" << section->get_name() << "\" (addr 0x" << std::hex
      << section->get_address() << std::dec << ", size " << section->get_size()
      << "):" << std::endl;

  // TODO: handle THUMB

  const char *data = section->get_data();
  size_t data_size = section->get_size() / sizeof(charm::arm::instr_t);

  for (charm::arm::addr_t i = 0; i < data_size;
       i += sizeof(charm::arm::instr_t)) {
    charm::arm::instr_t instr_raw;
    memcpy(&instr_raw, data + i, sizeof(charm::arm::instr_t));

    ofs << "\t0x" << std::hex
        << section->get_address() + i * sizeof(charm::arm::instr_t) << ": "
        << std::dec;

    auto instr = charm::arm::Instruction::decode(instr_raw);
    instr.dump(ofs);
    ofs << std::endl;
  }

  ofs << std::endl;
}

void dump_symtable(std::ofstream &ofs, ELFIO::elfio &elf,
                   ELFIO::section *section) {
  ofs << "SECTION \"" << section->get_name() << "\" (addr 0x" << std::hex
      << section->get_address() << std::dec << ", size " << section->get_size()
      << "):" << std::endl;

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

    ofs << "\t0x" << std::hex << value << ": " << std::dec << name << std::endl;
  }

  ofs << std::endl;
}
