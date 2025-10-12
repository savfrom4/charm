#include "arm.hpp"
#include "recomp.hpp"
#include "template.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>

namespace charm::recomp {

void Recompiler::step_emit(const std::string &output_dir) {
	if (!std::filesystem::exists(output_dir)) {
		std::filesystem::create_directory(output_dir);
	}

	emit_setup_project(output_dir);

	std::cout << "> Code ..." << std::endl;
	emit_code_header(output_dir);
	emit_code_source(output_dir);

	std::cout << "> Data ..." << std::endl;
	emit_data_header(output_dir);
	emit_data_source(output_dir);
}

void Recompiler::emit_setup_project(const std::filesystem::path &output_dir) {
	auto output_include_dir = output_dir / "include";
	auto output_src_dir = output_dir / "src";

	if (!std::filesystem::exists(output_include_dir)) {
		std::filesystem::create_directory(output_include_dir);
	}

	if (!std::filesystem::exists(output_src_dir)) {
		std::filesystem::create_directory(output_src_dir);
	}

	for (auto &file : std::filesystem::directory_iterator{"generator"}) {
		const auto filename = file.path().filename().string();

		// dont copy over templates
		if (filename.find(".tl") != std::string::npos) {
			continue;
		}

		// copy to /include
		if (filename.find(".hpp") != std::string::npos) {
			std::filesystem::copy_file(
			    file, output_include_dir / filename,
			    std::filesystem::copy_options::skip_existing);
			continue;
		}

		// copy to src/
		if (filename.find(".cpp") != std::string::npos) {
			std::filesystem::copy_file(
			    file, output_src_dir / filename,
			    std::filesystem::copy_options::skip_existing);
			continue;
		}

		// otherwise copy to root dir
		std::filesystem::copy_file(
		    file, output_dir / filename,
		    std::filesystem::copy_options::skip_existing);
	}
}

void Recompiler::emit_code_header(const std::filesystem::path &output_dir) {
	const auto code_hpp_path = output_dir / "include" / "code.hpp";

	std::ofstream ofs;
	ofs.exceptions(std::ofstream::failbit | std::ofstream::badbit);
	ofs.open(code_hpp_path);

	Template tl{"generator/code.hpp.tl"};

	// emit exported functions
	tl.format("exported_functions", [&](std::stringstream &ss) {
		for (auto &functions : _funs_exports) {
			ss << utils::sformat("\tvoid export_%s();",
			                     symbol_name_map(functions.second.name))
			   << std::endl;
		}
	});

	tl.format("external_functions", [&](std::stringstream &ss) {
		for (auto &functions : _funs_reloc) {
			if (!functions.second.is_external) {
				continue;
			}

			ss << utils::sformat("\tvoid external_%s();",
			                     symbol_name_map(functions.second.name))
			   << std::endl;
		}
	});

	ofs << tl.str();
}

void Recompiler::emit_code_source(const std::filesystem::path &output_dir) {
	const auto code_cpp_path = output_dir / "src" / "code.cpp";

	std::ofstream ofs;
	ofs.exceptions(std::ofstream::failbit | std::ofstream::badbit);
	ofs.open(code_cpp_path);

	Template tl{"generator/code.cpp.tl"};

	tl.format("got_mappings", [&](std::stringstream &ss) {
		for (auto &functions : _funs_reloc) {
			if (!functions.second.is_external) {
				continue;
			}

			ss << utils::sformat("INSTR(0x%X) { external_%s(); JUMP(r[LR]); }",
			                     functions.second.address,
			                     symbol_name_map(functions.second.name))
			   << std::endl;
		}
	});

	tl.format("sections", [&](std::stringstream &ss) {
		for (auto &section : _elf.sections) {
			if (!section_is_code(section.get())) {
				continue;
			}

			emit_code_section(ss, section.get());
		}
	});

	emit_code_address_mappings(tl);
	emit_code_stubs(tl);

	ofs << tl.str();
}

void Recompiler::emit_data_header(const std::filesystem::path &output_dir) {
	const auto code_hpp_path = output_dir / "include" / "data.hpp";

	std::ofstream ofs;
	ofs.exceptions(std::ofstream::failbit | std::ofstream::badbit);
	ofs.open(code_hpp_path);

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
		    << " std::array<" << data_type << ", " << data_size << "> g_"
		    << name << "_DATA;" << std::endl;

		ofs << "inline constexpr std::uint32_t " << name << "_ADDR = 0x"
		    << std::hex << section->get_address() << std::dec
		    << "; /* Virtual address of " << section->get_name() << " */"
		    << std::endl
		    << std::endl;
	}
}

void Recompiler::emit_data_source(const std::filesystem::path &output_dir) {
	const auto code_hpp_path = output_dir / "src" / "data.cpp";

	std::ofstream ofs;
	ofs.exceptions(std::ofstream::failbit | std::ofstream::badbit);
	ofs.open(code_hpp_path);

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
			    << section->get_size() << "> g_" << name << "_DATA = {"
			    << std::endl;

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
					arm::addr_t offset =
					    std::get<0>(mapping) - section->get_address();
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

void Recompiler::emit_code_address_mappings(Template &tl) {
	tl.format("address_map", [&](std::stringstream &ss) {
		for (auto &section : _elf.sections) {
			if (!section_is_data(section.get())) {
				continue;
			}

			auto name = symbol_name_map(section->get_name());
			std::transform(name.begin(), name.end(), name.begin(), ::toupper);

			ss << utils::sformat("\tif(address >= "
			                     "reinterpret_cast<std::uintptr_t>(g_%"
			                     "s_DATA.data()) && "
			                     "address < "
			                     "reinterpret_cast<std::uintptr_t>(g_%"
			                     "s_DATA.data()) + "
			                     "sizeof(g_%s_DATA)) {",
			                     name, name, name)
			   << std::endl;

			ss << utils::sformat(
			          "\t\treturn 0x%X + static_cast<uint32_t>(address "
			          "- "
			          "reinterpret_cast<uintptr_t>(g_%s_DATA.data()));",
			          section->get_address(), name)
			   << std::endl;

			ss << "\t}" << std::endl;
		}
	});

	tl.format("address_resolve", [&](std::stringstream &ss) {
		for (auto &section : _elf.sections) {
			if (!section_is_data(section.get())) {
				continue;
			}

			auto name = symbol_name_map(section->get_name());
			std::transform(name.begin(), name.end(), name.begin(), ::toupper);

			ss << utils::sformat("\tif(address >= 0x%X && address < 0x%X) {",
			                     section->get_address(),
			                     section->get_address() + section->get_size())
			   << std::endl;

			ss << utils::sformat("\t\treturn "
			                     "reinterpret_cast<std::uintptr_t>(&"
			                     "reinterpret_cast<const "
			                     "char*>(g_%s_DATA.data())[address - 0x%X]);",
			                     name, section->get_address())
			   << std::endl;

			ss << "\t}" << std::endl;
		}
	});
}

void Recompiler::emit_code_stubs(Template &tl) {
	tl.format("exported_functions", [&](std::stringstream &ss) {
		for (auto &functions : _funs_exports) {
			ss << utils::sformat("EXPORT(export_%s, 0x%X);",
			                     symbol_name_map(functions.second.name),
			                     functions.second.address)
			   << std::endl;
		}
	});

	tl.format("external_functions", [&](std::stringstream &ss) {
		for (auto &functions : _funs_reloc) {
			if (!functions.second.is_external) {
				continue;
			}

			ss << utils::sformat("STUB(external_%s);",
			                     symbol_name_map(functions.second.name))
			   << std::endl;
		}
	});
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
	if (!section) {
		return false;
	}

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
	if (!section) {
		return false;
	}

	if (!(section->get_flags() & ELFIO::SHF_EXECINSTR)) {
		return false;
	}

	if (!section->get_data()) {
		return false;
	}

	return true;
}

} // namespace charm::recomp
