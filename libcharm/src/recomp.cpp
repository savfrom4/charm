#include "libcharm/recomp.hpp"
#include <chrono>
#include <stdexcept>

namespace charm::recomp {

Recompiler::Recompiler(const std::string &elf_exe, bool minify) {
  if (!_elf.load(elf_exe))
    throw std::runtime_error("Not an elf file!");

  if (_elf.get_machine() != ELFIO::EM_ARM) {
    throw std::runtime_error("Not an arm binary!");
  }

  if (_elf.get_class() != ELFIO::ELFCLASS32) {
    throw std::runtime_error("Not an 32-bit binary!");
  }

  if (_elf.get_encoding() != ELFIO::ELFDATA2LSB) {
    throw std::runtime_error("Big-endian is not supported!");
  }

  _text = _elf.sections[".text"];
  if (!_text) {
    throw std::runtime_error("Missing .text!");
  }

  _plt = _elf.sections[".plt"];
  if (!_plt) {
    std::cout << "> Executable is missing plt table!" << std::endl;
  }

  _relplt = _elf.sections[".rel.plt"];
  if (!_relplt)
    _relplt = _elf.sections[".rela.plt"];

  _reldyn = _elf.sections[".rel.dyn"];
  if (!_reldyn)
    _reldyn = _elf.sections[".rela.dyn"];

  _dynsym = _elf.sections[".dynsym"];
  _minify = minify;
}

void Recompiler::emit(const std::string &output_dir) {
  std::cout << "******** ANALYZE ********" << std::endl;
  auto start = std::chrono::high_resolution_clock::now();
  step_analyze();

  std::cout << "Finished in "
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   (std::chrono::high_resolution_clock::now() - start))
                   .count()
            << " ms." << std::endl;

  std::cout << "********   EMIT  ********" << std::endl;
  start = std::chrono::high_resolution_clock::now();
  step_emit(output_dir);

  std::cout << "Finished in "
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   (std::chrono::high_resolution_clock::now() - start))
                   .count()
            << " ms." << std::endl;
}

} // namespace charm::recomp
