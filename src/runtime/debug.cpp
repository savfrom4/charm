#include "arch.hpp"
#include "runtime/memory.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <debug/debug.hpp>
#include <runtime/cpu.hpp>
#include <runtime/debug.hpp>

namespace charm::runtime {

Debugee::Debugee(CPUState &_ps, Memory &memory) : _cpu(_ps), _memory(memory) {
	_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (_socket < 0) {
		throw std::runtime_error(
		    "ExecutionDebugee ctor: failed to create socket.");
	}

	struct sockaddr_in address = {
	    .sin_family = AF_INET,
	    .sin_port = htons(CHARM_DEBUG_PORT),
	    .sin_addr = {.s_addr = INADDR_ANY},
	    .sin_zero = {0},
	};

	if (bind(_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
		throw std::runtime_error(
		    "ExecutionDebugee ctor: failed to bind socket.");
	}

	std::cout << "> Waitng for debugger on port " << CHARM_DEBUG_PORT << "..."
	          << std::endl;

	if (listen(_socket, 1) < 0) {
		throw std::runtime_error(
		    "ExecutionDebugee ctor: failed to listen on socket.");
	}

	socklen_t address_len;
	if (!(_connection =
	          accept(_socket, (struct sockaddr *)&address, &address_len))) {
		throw std::runtime_error(
		    "ExecutionDebugee ctor: failed to accept the connection.");
	}

	// disable nagle's algorithm
	int one = 1;
	if (setsockopt(_connection, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) <
	    0) {
		throw std::runtime_error(
		    "ExecutionDebugee ctor: failed to set NODELAY.");
	}

	std::cout << "> Connection established!" << std::endl;

	send_format("Waiting for user input...");
	_stall();
}

Debugee::~Debugee() {
	send_format("Reached program's end. Continuing will close the connection.");
	send_paused();
	_stall();

	if (_socket >= 0) {
		close(_socket);
	}

	if (_connection >= 0) {
		close(_connection);
	}
}

void Debugee::next() {
	if (flags & NEXT) {
		flags &= ~NEXT; // clear flag

		send_message();
		send_paused();
	}

	_stall();
}

void Debugee::skip() {
	bool is_breakpoint = _breakpoints.count(_cpu.r[PC] - 8);

	if (is_breakpoint || flags & SKIP) {
		flags &= ~SKIP; // clear flag

		send_message();

		if (is_breakpoint) {
			send_format("Breakpoint hit.");
		}

		send_paused();
	} else if (flags & NEXT) {
		send_message();
		send_paused();
	}

	_stall();
}

void Debugee::send_paused() {
	flags |= PAUSED; // pause

	std::uint32_t length = 0; // sending length 0 is pause request
	write(_connection, &length, sizeof(length));

	_stall();
}

void Debugee::send_message() {
	auto temp_buffer_ptr = _temp_buffer.data();

	std::uint32_t length = std::strlen(temp_buffer_ptr);
	std::memmove(temp_buffer_ptr + sizeof(length), temp_buffer_ptr, length + 1);

	length = htonl(length);
	std::memcpy(temp_buffer_ptr, &length, sizeof(length));
	length = ntohl(length);

	write(_connection, temp_buffer_ptr, length + sizeof(length));
}

bool Debugee::_poll(int timeout) {
	struct pollfd fd = {
	    .fd = _connection,
	    .events = POLLIN,
	    .revents = 0,
	};

	int result = ::poll(&fd, 1, timeout);
	if (result == 0) {
		return false; /* nothing, still wait */
	}

	if (!result) {
		throw std::runtime_error(
		    "Debugee::poll: invalid file descriptor (connection is lost...?).");
	}

	return (fd.revents & POLLIN);
}

void Debugee::_process(int timeout) {
	auto temp_buffer_ptr = _temp_buffer.data();

	while (_poll(timeout)) {
		ssize_t bytes_read =
		    ::read(_connection, temp_buffer_ptr, sizeof(_temp_buffer));

		if (!bytes_read) {
			throw std::runtime_error("Debugee::process: connection is lost.");
		}

		_accum_buffer.insert(_accum_buffer.end(), temp_buffer_ptr,
		                     temp_buffer_ptr + bytes_read);
	}

	// try to read as much as we can
	while (1) {
		if (!_accum_buffer.size()) {
			return;
		}

		// read command type
		if (_command == debug::Command::NONE) {
			std::memcpy(&_command, _accum_buffer.data(), sizeof(_command));
			_accum_buffer.erase(_accum_buffer.begin());
		}

		if ((int)_command >= debug::COMMAND_SIZE_TABLE.size()) {
			throw std::runtime_error("Debugee:process: invalid command type: " +
			                         std::to_string((int)_command));
		}

		// not enough data
		const auto command_size = debug::COMMAND_SIZE_TABLE[(int)_command];
		if (_accum_buffer.size() < command_size) {
			return;
		}

		_process_command();

		_accum_buffer.erase(_accum_buffer.begin(),
		                    _accum_buffer.begin() + command_size);
		_command = debug::Command::NONE;
	}
}

void Debugee::_process_command() {
	switch (_command) {
	case debug::Command::BREAK: {
		Word value;
		std::memcpy(&value, _accum_buffer.data(), sizeof(value));
		value = ntohl(value);

		if (_breakpoints.count(value)) {
			_breakpoints.erase(value);
			send_format("Breakpoint at 0x%X removed.", value);
			break;
		}

		_breakpoints.emplace(value);
		send_format("Breakpoint at 0x%X set.", value);
		break;
	}

	case debug::Command::NEXT: {
		flags |= NEXT;
		flags &= ~PAUSED;
		break;
	}

	case debug::Command::SKIP: {
		flags |= SKIP;
		flags &= ~PAUSED;
		break;
	}

	case debug::Command::PAUSE_MODE: {
		Byte value;
		std::memcpy(&value, _accum_buffer.data(), sizeof(value));

		if (value) {
			flags |= PAUSED;
		} else {
			flags &= ~PAUSED;
		}
		break;
	}

	case debug::Command::PRINT_REGISTER: {
		Word value;
		std::memcpy(&value, _accum_buffer.data(), sizeof(value));
		value = ntohl(value);

		send_format("r%d=0x%X (%u)", value, _cpu.r[value], _cpu.r[value]);
		break;
	}

	case debug::Command::PRINT_AT_ADDRESS: {
		Word address;
		std::memcpy(&address, _accum_buffer.data(), sizeof(address));
		address = ntohl(address);

		Byte value;
		_memory.access().load(address, &value, sizeof(value));

		send_format("0x%X=0x%X (%u)", address, value, value);
		break;
	}

	case debug::Command::DUMP:
	case debug::Command::RESTORE:
		break;

	default:
		throw std::runtime_error(
		    "Debugee::process_command: invalid command type.");
	}
}

void Debugee::_stall() {
	while (flags & PAUSED)
		_process(30); // wait for continue
}
} // namespace charm::runtime
