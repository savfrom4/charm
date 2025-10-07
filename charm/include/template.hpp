#pragma once
#include "utils.hpp"
#include <algorithm>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace charm {

class Template {
  public:
	inline Template(const std::string &filename) {
		std::stringstream ss;

		std::ifstream ifs{filename};
		if (ifs.fail()) {
			throw std::invalid_argument(
			    "Template::ctor: Failed to open file \"" + filename + "\".");
		}

		ss << ifs.rdbuf();
		ifs.close();
		_contents = ss.str();

		static const std::string TOKEN_START = "/*%";
		static const std::string TOKEN_END = "%*/";

		for (auto it = _contents.find(TOKEN_START); it != std::string::npos;
		     it = _contents.find(TOKEN_START, it + 1)) {
			auto end = _contents.find(TOKEN_END, it);
			if (end == std::string::npos) {
				throw std::runtime_error("Template::ctor: Trailing " +
				                         TOKEN_START + ".");
			}

			auto start = it + TOKEN_START.size();
			auto token = _contents.substr(start, end - start);

			// remove all spaces
			token.erase(std::remove_if(token.begin(), token.end(),
			                           [](char c) { return isspace(c); }),
			            token.end());

			// to cover full token with prefix/postfix
			end += TOKEN_END.size();

			// shift values after the end of current token
			for (auto &token : _tokens) {
				if (token.second <= end) {
					continue;
				}

				token.second -= end - it;
			}

			// remove the whole token & store it
			_contents.erase(_contents.begin() + it, _contents.begin() + end);
			_tokens[token] = it;
		}
	}

	template <typename... Args>
	inline Template &format(const std::string &token,
	                        std::function<void(std::stringstream &)> cb) {
		std::stringstream ss;
		cb(ss);

		format(token, ss.str());
		return *this;
	}

	template <typename... Args>
	inline Template &format(const std::string &token, const std::string &fmt,
	                        Args... args) {
		format(token, utils::sformat(fmt, args...));
		return *this;
	}

	inline Template &format(const std::string &token,
	                        const std::string &value) {
		// search for the token
		const auto it = _tokens.find(token);
		if (it == _tokens.end()) {
			throw std::invalid_argument("Template::arg: Token not found: \"" +
			                            token + "\"!");
		}

		// insert our value
		_contents.insert(_contents.begin() + it->second, value.begin(),
		                 value.end());

		// shift values after the end of current token
		for (auto &token : _tokens) {
			if (token.second <= it->second) {
				continue;
			}

			token.second += value.size();
		}

		_tokens.erase(it);
		return *this;
	}

	const std::string &str() const { return _contents; }

  private:
	std::string _contents;
	std::unordered_map<std::string, std::string::size_type> _tokens;
};

} // namespace charm
