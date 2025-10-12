#pragma once
#include "debug.hpp"
#include <array>
#include <cstdint>
#include <mutex>
#include <type_traits>

// -------------------------------------
// ------------- OPTIONS ---------------
// -------------------------------------

#ifndef LAYER_ELF_SECTIONS_SIZE
#define LAYER_ELF_SECTIONS_SIZE                                                \
	(/*<% sections_size %>*/) // Size of the elf sections (word-aligned)
#endif
#define LAYER_ELF_SECTIONS_BASE (0)

#ifndef LAYER_MEMORY_SIZE
#define LAYER_MEMORY_SIZE                                                      \
	(1024 * 1024 * 64) // Size of the memory in bytes (word-aligned)
#endif
#define LAYER_MEMORY_BASE (LAYER_ELF_SECTIONS_SIZE)

#ifndef LAYER_STACK_SIZE
#define LAYER_STACK_SIZE                                                       \
	(1024 * 1024 * 16) // Size of the stack in bytes (word-aligned)
#endif
#define LAYER_STACK_BASE (LAYER_ELF_SECTIONS_SIZE + LAYER_MEMORY_SIZE)

#ifndef LAYER_MEMORY_BLOCK_SIZE
#define LAYER_MEMORY_BLOCK_SIZE (64) // Min allocation in bytes (word-aligned)
#endif

namespace layer {

typedef std::uint32_t virtual_addr_t;
typedef std::uint32_t virtual_size_t;

class VirtualMemory;

class VirtualAddressAccess {
  public:
	inline VirtualAddressAccess(const VirtualAddressAccess &) = delete;
	inline VirtualAddressAccess &
	operator=(const VirtualAddressAccess &) = delete;

	template <typename T>
	VirtualAddressAccess &load(virtual_addr_t address, T buffer,
	                           virtual_size_t size) {
		static_assert(std::is_pointer_v<T>, "T must be a pointer!");
		std::memcpy(buffer, _impl_access(address, false), size);
	}

	template <typename T>
	VirtualAddressAccess &store(virtual_addr_t address, T buffer,
	                            virtual_size_t size) {
		static_assert(std::is_pointer_v<T>, "T must be a pointer!");
		std::memcpy(_impl_access(address, true), buffer, size);
	}

	inline virtual_addr_t alloc(virtual_size_t size) {
		return _impl_alloc(size);
	}

	inline void free(virtual_addr_t address) { return _impl_free(address); }

  private:
	friend class VirtualMemory;

	VirtualMemory &_vm;
	std::lock_guard<std::mutex> _lock;

	virtual_addr_t _impl_alloc(virtual_size_t size);
	void _impl_free(virtual_addr_t ptr);
	void *_impl_access(virtual_addr_t address, bool read_write);

	VirtualAddressAccess(VirtualMemory &vm);
};

class VirtualMemory {
  public:
	VirtualMemory(Debugee &dbe);

	inline VirtualAddressAccess access() { return VirtualAddressAccess{*this}; }

  private:
	friend class VirtualAddressAccess;

#ifdef LAYER_DEBUG
	Debugee &_dbe;
#endif
	std::vector<std::uint8_t> _memory;
	std::mutex _mutex;
};

} // namespace layer
