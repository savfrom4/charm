#include <iostream>

const std::string VERSION = "1.0.0";

void show_help();

int main(int argc, char **argv) {
  if (argc < 2) {
    show_help();
    return 1;
  }

  return 0;
}

void show_help() {
  std::cout << "charm-dbg v" << VERSION
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
