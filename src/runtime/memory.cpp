#include "arch.hpp"
#include "utils.hpp"
#include <runtime/memory.hpp>
#include <stdexcept>

namespace charm::runtime {

struct HeapBlock {
	bool allocated;
	Word size;
};

Memory::Memory(MemoryConfig config) : _config(config) {
	// word-align sizes
	_heap_base = (_config.elf_size + 3) & ~3;
	_stack_base = _heap_base + (_config.heap_size + 3) & ~3;

	_data.resize(_stack_base + ((_config.stack_size + 3) & ~3));

	const HeapBlock blk = {
	    .allocated = false,
	    .size = _config.heap_blk_size,
	};
	const auto blk_iter = _config.heap_blk_size + sizeof(HeapBlock);

	for (Word i = 0; i < _config.heap_size; i += blk_iter) {
		// discard whats outside the range
		if (i + blk_iter >= _config.heap_size) {
			break;
		}

		std::memcpy(&_data[_heap_base + i], &blk, sizeof(blk));
	}
}

MemoryAccessGuard::MemoryAccessGuard(std::mutex &mutex, Memory &memory)
    : std::lock_guard<std::mutex>(mutex), _memory(memory) {}

Word MemoryAccessGuard::_impl_halloc(Word size) {
	if (!size) {
		return 0;
	}

	size = (size + 3) & ~3; // word-align

	// we iterate trying to find a free block
	Byte *ptr = _memory._data.data() + _memory._heap_base;
	const Byte *end = ptr + _memory._config.heap_size;

	while (ptr < end) {
		HeapBlock blk;
		std::memcpy(&blk, ptr, sizeof(blk));

		if (blk.allocated) {
			ptr += blk.size + sizeof(blk);
			continue;
		}

		// check if block is enough in size
		if (blk.size >= size) {
			// maybe even too big. calculate the diff
			int64_t diff = blk.size - size;

			// if its more than block sizes, split the block in two
			if (diff >= _memory._config.heap_blk_size) {
				Byte *next_blk_ptr = ptr + diff + sizeof(HeapBlock);
				const HeapBlock next_blk = {
				    .allocated = false,
				    .size = static_cast<Word>(diff),
				};

				std::memcpy(next_blk_ptr, &next_blk, sizeof(next_blk));
				blk.size -= diff;
			}

			blk.allocated = true;
			std::memcpy(ptr, &blk, sizeof(blk));
			return (Word)(ptr + sizeof(blk) - _memory._data.data());
		}

		// dont give up yet, try to combine multiple blocks
		Byte *next_blk_ptr = ptr + blk.size + sizeof(HeapBlock);
		Word accumulated_size = blk.size, n = 0;
		bool found = false;

		while (next_blk_ptr < end) {
			HeapBlock next_blk;
			std::memcpy(&next_blk, next_blk_ptr, sizeof(next_blk));

			// we hit a taken block
			if (next_blk.allocated) {
				break;
			}

			// hey maybe we reached our target!
			accumulated_size += next_blk.size;
			n++;
			if (accumulated_size >= size) {
				found = true;
				break;
			}

			next_blk_ptr += next_blk.size + sizeof(HeapBlock);
		}

		if (found) {
			blk.size = accumulated_size + n * sizeof(HeapBlock);
			blk.allocated = true;

			std::memcpy(ptr, &blk, sizeof(blk));
			return (Word)(ptr + sizeof(blk) - _memory._data.data());
		}

		ptr += blk.size + sizeof(blk);
	}

	return 0;
}

void MemoryAccessGuard::_impl_hfree(Word address) {
	Byte *base = reinterpret_cast<Byte *>(_impl_access(address, true)) -
	             sizeof(HeapBlock);

	HeapBlock blk;
	std::memcpy(&blk, base, sizeof(blk));

	blk.allocated = false;
	std::memcpy(base, &blk, sizeof(blk));
}

void *MemoryAccessGuard::_impl_access(Word address, bool read_write) {
	if (!address || address >= _memory._data.size()) {
		throw std::runtime_error(
		    utils::sformat("%s: Segmentation fault.", __func__));
	}

	return reinterpret_cast<void *>(&_memory._data[address]);
}

} // namespace charm::runtime
