#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>

#include <isa/arm.hpp>
#include <recompiler/recomp.hpp>
#include <recompiler/template.hpp>
#include <utils.hpp>

namespace charm::recomp {

void Recompiler::_step_emit(const std::string &output_dir) {
	if (!std::filesystem::exists(output_dir)) {
		std::filesystem::create_directory(output_dir);
	}

	_emit_setup_project(output_dir);

	std::cout << "> Code ..." << std::endl;
	_emit_code_header(output_dir);
	_emit_code_source(output_dir);

	std::cout << "> Data ..." << std::endl;
	_emit_data_source(output_dir);
}

void Recompiler::_emit_setup_project(const std::filesystem::path &output_dir) {
	auto output_include_dir = output_dir / "include";
	auto output_src_dir = output_dir / "src";

	if (!std::filesystem::exists(output_include_dir)) {
		std::filesystem::create_directory(output_include_dir);
	}

	if (!std::filesystem::exists(output_src_dir)) {
		std::filesystem::create_directory(output_src_dir);
	}

	for (auto &file : std::filesystem::directory_iterator{"templates"}) {
		const auto filename = file.path().filename().string();

		// dont copy over templates
		if (filename.find(".tl") != std::string::npos) {
			continue;
		}

		// otherwise copy to root dir
		std::filesystem::copy_file(
		    file, output_dir / filename,
		    std::filesystem::copy_options::skip_existing);
	}
}

void Recompiler::_emit_code_header(const std::filesystem::path &output_dir) {
	const auto code_hpp_path = output_dir / "include" / "code.hpp";

	std::ofstream ofs;
	ofs.exceptions(std::ofstream::failbit | std::ofstream::badbit);
	ofs.open(code_hpp_path);

	Template tl{"code.hpp.tl"};

	// emit exported functions
	tl.format("exported_functions", [&](std::stringstream &ss) {
		for (auto &functions : _funs_exports) {
			ss << utils::sformat("\tvoid export_%s();",
			                     _symbol_name_map(functions.second.name))
			   << std::endl;
		}
	});

	tl.format("external_functions", [&](std::stringstream &ss) {
		for (auto &functions : _funs_reloc) {
			if (!functions.second.is_external) {
				continue;
			}

			ss << utils::sformat("\tvoid external_%s();",
			                     _symbol_name_map(functions.second.name))
			   << std::endl;
		}
	});

	ofs << tl.str();
}

void Recompiler::_emit_code_source(const std::filesystem::path &output_dir) {
	const auto code_cpp_path = output_dir / "src" / "code.cpp";

	std::ofstream ofs;
	ofs.exceptions(std::ofstream::failbit | std::ofstream::badbit);
	ofs.open(code_cpp_path);

	Template tl{"code.cpp.tl"};

	tl.format("got_mappings", [&](std::stringstream &ss) {
		for (auto &functions : _funs_reloc) {
			if (!functions.second.is_external) {
				continue;
			}

			ss << utils::sformat("INSTR(0x%X) { external_%s(); JUMP(r[LR]); }",
			                     functions.second.address,
			                     _symbol_name_map(functions.second.name))
			   << std::endl;
		}
	});

	tl.format("sections", [&](std::stringstream &ss) {
		for (auto &section : _elf.sections) {
			if (!_section_is_code(section.get())) {
				continue;
			}

			_emit_code_section(ss, section.get());
		}
	});

	_emit_code_stubs(tl);

	ofs << tl.str();
}

void Recompiler::_emit_data_source(const std::filesystem::path &output_dir) {
	const auto code_hpp_path = output_dir / "src" / "data.cpp";

	std::ofstream ofs;
	ofs.exceptions(std::ofstream::failbit | std::ofstream::badbit);
	ofs.open(code_hpp_path);

	Template tl{"data.cpp.tl"};

	tl.format("data_arrays", [&](std::stringstream &ss) {
		for (auto &section : _elf.sections) {
			if (!_section_is_data(section.get())) {
				continue;
			}

			const std::uint8_t *data =
			    reinterpret_cast<const std::uint8_t *>(section->get_data());

			auto name = _symbol_name_map(section->get_name());
			std::transform(name.begin(), name.end(), name.begin(), ::toupper);

			// for non-got table we just write raw bytes or 0es
			if (section->get_name().find(".got") == std::string::npos) {
				ss << ((section->get_flags() & ELFIO::SHF_WRITE)
				           ? "std::array<std::uint8_t, "
				           : "const std::array<std::uint8_t, ")
				   << section->get_size() << "> " << name << "_DATA = {"
				   << std::endl;

				ss << "\t";
				for (auto i = 0; i < section->get_size(); i++) {
					ss << (data ? static_cast<int>(data[i]) : 0) << ", ";

					if (i % 8 == 7) {
						ss << std::endl;
						ss << "\t";
					}
				}
			} else { // for got we map addresses that we know
				ss << ((section->get_flags() & ELFIO::SHF_WRITE)
				           ? "std::array<std::uint32_t, "
				           : "const std::array<std::uint32_t, ")
				   << section->get_size() / sizeof(Word) << "> " << name
				   << "_DATA = {" << std::endl;

				ss << std::hex;

				// map addresses
				for (auto i = 0; i < section->get_size(); i += sizeof(Word)) {
					Word mapped_address = 0;

					for (auto &mapping : _got_mappings) {
						Word offset =
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

			ss << "};" << std::endl << "};" << std::endl;
		}
	});

	tl.format("sections", [&](std::stringstream &ss) {
		for (auto &section : _elf.sections) {
			if (!_section_is_data(section.get())) {
				continue;
			}

			std::string name = _symbol_name_map(section->get_name());
			std::transform(name.begin(), name.end(), name.begin(), ::toupper);

			ss << utils::sformat(
			          "\taccess.store(0x%X, %s_DATA, sizeof(%s_DATA));",
			          (Word)section->get_address(), name, name)
			   << std::endl;
		}
	});

	ofs << tl.str();
}

void Recompiler::_emit_code_stubs(Template &tl) {
	tl.format("exported_functions", [&](std::stringstream &ss) {
		for (auto &functions : _funs_exports) {
			ss << utils::sformat("EXPORT(export_%s, 0x%X);",
			                     _symbol_name_map(functions.second.name),
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
			                     _symbol_name_map(functions.second.name))
			   << std::endl;
		}
	});
}

std::string Recompiler::_symbol_name_map(const std::string &symbol) {
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

bool Recompiler::_section_is_data(const ELFIO::section *section) {
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

bool Recompiler::_section_is_code(const ELFIO::section *section) {
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
