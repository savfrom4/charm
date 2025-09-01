#include "liblayer.hpp"

struct Block {
  enum : uint32_t {
    FREE,
    ALLOCATED,
    FREED,
  } state;

  uint32_t size;
};

inline uint32_t ExecutionState::address_map(uintptr_t addr) {
  if (addr >= reinterpret_cast<uintptr_t>(stack) &&
      addr < reinterpret_cast<uintptr_t>(stack) + LIBLAYER_STACK_SIZE) {
    return LIBLAYER_STACK_BASE +
           static_cast<uint32_t>(addr - reinterpret_cast<uintptr_t>(stack));
  }

  if (addr >= reinterpret_cast<uintptr_t>(memory) &&
      addr < reinterpret_cast<uintptr_t>(memory) + LIBLAYER_MEMORY_SIZE) {
    return LIBLAYER_STACK_SIZE +
           static_cast<uint32_t>(addr - reinterpret_cast<uintptr_t>(memory));
  }

  return 0;
}

inline uintptr_t ExecutionState::address_resolve(uint32_t addr) {
  if (addr >= LIBLAYER_STACK_BASE &&
      addr < LIBLAYER_STACK_BASE + LIBLAYER_STACK_SIZE) {
    return reinterpret_cast<uintptr_t>(&stack[addr - LIBLAYER_STACK_BASE]);
  }

  if (addr >= LIBLAYER_MEMORY_BASE &&
      addr < LIBLAYER_MEMORY_BASE + LIBLAYER_MEMORY_SIZE) {
    return reinterpret_cast<uintptr_t>(&stack[addr - LIBLAYER_MEMORY_BASE]);
  }

  return 0;
}

void *ExecutionState::alloc(uint32_t size) {
  if (!size) {
    return nullptr;
  }

  // we iterate trying to find a free block
  uint8_t *ptr = memory;
  uint8_t *end = memory + LIBLAYER_MEMORY_SIZE;

  void *allocated = nullptr;

  while (ptr < end) {
    Block blk;
    memcpy(&blk, ptr, sizeof(blk));

    switch (blk.state) {
    case Block::FREE:
      // if the block is free, that means we're at the end. Try to allocate more
      if (ptr + sizeof(blk) + size >= end) {
        return NULL; // cannot allocate anymore
      }

      blk.state = Block::ALLOCATED;
      blk.size = size;
      memcpy(ptr, &blk, sizeof(blk));

      return reinterpret_cast<void *>(ptr + sizeof(blk));

    case Block::FREED: {
      if (size > blk.size) {
        continue; // block is too small
      }

      // search for nearby bloks that are freed too
      uint8_t *freed_ptr = ptr + sizeof(blk) + blk.size;
      Block freed_blk;
      while (freed_ptr < end) {
        memcpy(&freed_blk, ptr, sizeof(blk));

        // not continious anymore, break
        if (freed_blk.state != Block::FREED) {
          break;
        }

        blk.size += sizeof(freed_blk) + freed_blk.size; // grow it
      }

      blk.state = Block::ALLOCATED;
      memcpy(ptr, &blk, sizeof(blk));

      return reinterpret_cast<void *>(ptr + sizeof(blk));
    }

    case Block::ALLOCATED:
      ptr += sizeof(blk) + blk.size;
      continue;
    }
  }

  return allocated;
}

void ExecutionState::free(void *p) {
  if (!p) {
    return;
  }
}
