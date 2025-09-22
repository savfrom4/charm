#include "liblayer.hpp"
#include <cstring>
#include <iostream>
#include <mutex>
#include <ostream>

#define BLOCK_SIZE (64)                         // Min allocation
#define BLOCK_ITER (BLOCK_SIZE + sizeof(Block)) // + sizeof(Block)

struct Block {
  bool allocated;
  uint32_t size;
};

namespace layer {

inline uint32_t ExecutionState::memory_map(uintptr_t address) {
  // stack
  if (address >= reinterpret_cast<uintptr_t>(stack) &&
      address < reinterpret_cast<uintptr_t>(stack) + LAYER_STACK_SIZE) {
    return LAYER_STACK_BASE +
           static_cast<uint32_t>(address - reinterpret_cast<uintptr_t>(stack));
  }

  // memory
  else if (address >= reinterpret_cast<uintptr_t>(memory) &&
           address < reinterpret_cast<uintptr_t>(memory) + LAYER_MEMORY_SIZE) {
    return LAYER_MEMORY_BASE +
           static_cast<uint32_t>(address - reinterpret_cast<uintptr_t>(memory));
  }

  return 0;
}

inline uintptr_t ExecutionState::memory_resolve(uint32_t address) {
  // stack
  if (address >= LAYER_STACK_BASE &&
      address < LAYER_STACK_BASE + LAYER_STACK_SIZE) {
    return reinterpret_cast<uintptr_t>(&stack[address - LAYER_STACK_BASE]);
  }

  // memory
  else if (address >= LAYER_MEMORY_BASE &&
           address < LAYER_MEMORY_BASE + LAYER_MEMORY_SIZE) {
    return reinterpret_cast<uintptr_t>(&memory[address - LAYER_MEMORY_BASE]);
  }

  return 0;
}

void ExecutionState::memory_init() {
  memset(memory, 0, LAYER_MEMORY_SIZE);

  const Block blk = {
      .allocated = false,
      .size = BLOCK_SIZE,
  };

  for (uint32_t i = 0; i < LAYER_MEMORY_SIZE; i += BLOCK_ITER) {
    // discard whats outside the range
    if (i + BLOCK_ITER >= LAYER_MEMORY_SIZE) {
      break;
    }

    memcpy(&memory[i], &blk, sizeof(blk));
  }
}

void *ExecutionState::memory_alloc(uint32_t size) {
  if (!size) {
    return nullptr;
  }

  size = (size + 3) & ~3; // word-align

  std::lock_guard lock{memory_mutex};

  // we iterate trying to find a free block
  uint8_t *ptr = memory;
  uint8_t *end = memory + LAYER_MEMORY_SIZE;

  while (ptr < end) {
    Block blk;
    memcpy(&blk, ptr, sizeof(blk));

    if (blk.allocated) {
      ptr += blk.size + sizeof(blk);
      continue;
    }

    // check if block is enough in size
    if (blk.size >= size) {
      // maybe even too big. calculate the diff
      int64_t diff = blk.size - size;

      // if its more than block sizes, split the block in two
      if (diff >= BLOCK_SIZE) {
        uint8_t *next_blk_ptr = ptr + diff + sizeof(Block);
        const Block next_blk = {
            .allocated = false,
            .size = static_cast<uint32_t>(diff),
        };

        memcpy(next_blk_ptr, &next_blk, sizeof(next_blk));

        blk.size -= diff;
      }

      blk.allocated = true;
      memcpy(ptr, &blk, sizeof(blk));
      return ptr + sizeof(blk);
    }

    // dont give up yet, try to combine multiple blocks
    uint8_t *next_blk_ptr = ptr + blk.size + sizeof(Block);
    size_t accumulated_size = blk.size, n = 0;
    bool found = false;

    while (next_blk_ptr < end) {
      Block next_blk;
      memcpy(&next_blk, next_blk_ptr, sizeof(next_blk));

      // we hit a taken block
      if (next_blk.allocated) {
        break;
      }

      // hey maybe we reached our target!
      accumulated_size += next_blk.size;
      n++;
      if (accumulated_size >= size) {
        std::cout << "fa" << std::endl;
        found = true;
        break;
      }

      next_blk_ptr += next_blk.size + sizeof(Block);
    }

    if (found) {
      std::cout << "ok" << std::endl;
      blk.size = accumulated_size + n * sizeof(Block);
      blk.allocated = true;

      memcpy(ptr, &blk, sizeof(blk));
      return ptr + sizeof(blk);
    }

    ptr += blk.size + sizeof(blk);
  }

  return nullptr;
}

void ExecutionState::memory_free(void *p) {
  if (!p) {
    return;
  }

  std::lock_guard lock{memory_mutex};
  char *base = reinterpret_cast<char *>(p) - sizeof(Block);

  Block blk;
  memcpy(&blk, base, sizeof(Block));

  blk.allocated = false;
  memcpy(base, &blk, sizeof(Block));
}

} // namespace layer
