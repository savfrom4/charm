#pragma once
#include <cstdint>
#include <cstring>
#include <mutex>
#include <type_traits>
#include <vector>

#include <arch.hpp>

namespace charm::runtime {

class Memory;

struct MemoryConfig {
	Word total_size;
	Word heap_base, heap_size, heap_blk_size;
	Word stack_base, stack_size;
};

class MemoryAccessGuard : public std::lock_guard<std::mutex> {
  public:
	MemoryAccessGuard(std::mutex &mutex, Memory &memory);

	template <typename T> T *ptr(Word address) {
		static_assert(std::is_pointer_v<T>, "T must be a pointer!");
		return reinterpret_cast<T *>(
		    _impl_access(address, !std::is_const_v<std::remove_pointer_t<T>>));
	}

	template <typename T>
	MemoryAccessGuard &load(Word address, T buffer, Word size) {
		static_assert(std::is_pointer_v<T>, "T must be a pointer!");
		std::memcpy(buffer, _impl_access(address, false), size);
	}

	template <typename T>
	MemoryAccessGuard &store(Word address, T buffer, Word size) {
		static_assert(std::is_pointer_v<T>, "T must be a pointer!");
		std::memcpy(_impl_access(address, true), buffer, size);
	}

	inline Word halloc(Word size) { return _impl_halloc(size); }
	inline void hfree(Word address) { return _impl_hfree(address); }

  private:
	Memory &_memory;

	void *_impl_access(Word address, bool read_write);
	Word _impl_halloc(Word size);
	void _impl_hfree(Word ptr);
};

class Memory {
  public:
	Memory(MemoryConfig config);

	inline MemoryAccessGuard access() {
		return MemoryAccessGuard{_mutex, *this};
	}

  private:
	friend class MemoryAccessGuard;

	MemoryConfig _config;
	std::vector<std::uint8_t> _data;
	std::mutex _mutex;
};

} // namespace charm::runtime
