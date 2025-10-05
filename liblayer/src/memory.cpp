#include "liblayer/debug.hpp"
#include "liblayer/execution_state.hpp"
#include <cstring>
#include <iostream>
#include <mutex>

#define BLOCK_ITER (LAYER_MEMORY_BLOCK_SIZE + sizeof(Block)) // + sizeof(Block)

struct Block {
  bool allocated;
  std::uint32_t size;
};

namespace layer {

std::uint32_t ExecutionState::address_map_raw(std::uintptr_t address) {
  auto stack_ptr = stack.data();
  auto memory_ptr = memory.data();

  // stack
  if (address >= reinterpret_cast<std::uintptr_t>(stack_ptr) &&
      address <
          reinterpret_cast<std::uintptr_t>(stack_ptr) + LAYER_STACK_SIZE) {
    return LAYER_STACK_BASE +
           static_cast<std::uint32_t>(
               address - reinterpret_cast<std::uintptr_t>(stack_ptr));
  }

  // memory
  else if (address >= reinterpret_cast<std::uintptr_t>(memory_ptr) &&
           address < reinterpret_cast<std::uintptr_t>(memory_ptr) +
                         LAYER_MEMORY_SIZE) {
    return LAYER_MEMORY_BASE +
           static_cast<std::uint32_t>(
               address - reinterpret_cast<std::uintptr_t>(memory_ptr));
  }

  return 0;
}

std::uintptr_t ExecutionState::address_resolve_raw(std::uint32_t address) {
  // stack
  if (address >= LAYER_STACK_BASE &&
      address < LAYER_STACK_BASE + LAYER_STACK_SIZE) {
    return reinterpret_cast<std::uintptr_t>(&stack[address - LAYER_STACK_BASE]);
  }

  // memory
  else if (address >= LAYER_MEMORY_BASE &&
           address < LAYER_MEMORY_BASE + LAYER_MEMORY_SIZE) {
    return reinterpret_cast<std::uintptr_t>(
        &memory[address - LAYER_MEMORY_BASE]);
  }

  return 0;
}

void ExecutionState::memory_init() {
#if LAYER_STACK_SIZE > LAYER_STACK_ON_STACK_LIMIT
  stack.resize(LAYER_STACK_SIZE);
  std::memset(stack.data(), 0, LAYER_STACK_SIZE);
#endif

  memory.resize(LAYER_MEMORY_SIZE);
  std::memset(memory.data(), 0, LAYER_MEMORY_SIZE);

  const Block blk = {
      .allocated = false,
      .size = LAYER_MEMORY_BLOCK_SIZE,
  };

  for (uint32_t i = 0; i < LAYER_MEMORY_SIZE; i += BLOCK_ITER) {
    // discard whats outside the range
    if (i + BLOCK_ITER >= LAYER_MEMORY_SIZE) {
      break;
    }

    std::memcpy(&memory[i], &blk, sizeof(blk));
  }

  LAYER_DBE_LOG(*this, "virtual stack: begin 0x%X, end 0x%X", LAYER_STACK_BASE,
                LAYER_STACK_BASE + LAYER_STACK_SIZE);

  LAYER_DBE_LOG(*this, "virtual memory: begin 0x%X, end 0x%X",
                LAYER_MEMORY_BASE, LAYER_MEMORY_BASE + LAYER_MEMORY_SIZE);
}

void *ExecutionState::memory_alloc_raw(uint32_t size) {
  if (!size) {
    return nullptr;
  }

  size = (size + 3) & ~3; // word-align

  std::cout << size << std::endl;

  std::lock_guard lock{_memory_lock};

  // we iterate trying to find a free block
  uint8_t *ptr = memory.data();
  uint8_t *end = memory.data() + LAYER_MEMORY_SIZE;

  while (ptr < end) {
    Block blk;
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
      if (diff >= LAYER_MEMORY_BLOCK_SIZE) {
        uint8_t *next_blk_ptr = ptr + diff + sizeof(Block);
        const Block next_blk = {
            .allocated = false,
            .size = static_cast<uint32_t>(diff),
        };

        std::memcpy(next_blk_ptr, &next_blk, sizeof(next_blk));
        blk.size -= diff;
      }

      blk.allocated = true;
      std::memcpy(ptr, &blk, sizeof(blk));
      std::cout << reinterpret_cast<std::uintptr_t>(ptr + sizeof(blk))
                << std::endl;
      return ptr + sizeof(blk);
    }

    // dont give up yet, try to combine multiple blocks
    uint8_t *next_blk_ptr = ptr + blk.size + sizeof(Block);
    size_t accumulated_size = blk.size, n = 0;
    bool found = false;

    while (next_blk_ptr < end) {
      Block next_blk;
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

      next_blk_ptr += next_blk.size + sizeof(Block);
    }

    if (found) {
      blk.size = accumulated_size + n * sizeof(Block);
      blk.allocated = true;

      std::memcpy(ptr, &blk, sizeof(blk));
      std::cout << reinterpret_cast<std::uintptr_t>(ptr + sizeof(blk))
                << std::endl;
      return ptr + sizeof(blk);
    }

    ptr += blk.size + sizeof(blk);
  }

  std::cout << 0 << std::endl;
  return nullptr;
}

void ExecutionState::memory_free(void *p) {
  if (!p) {
    return;
  }

  std::lock_guard lock{_memory_lock};
  char *base = reinterpret_cast<char *>(p) - sizeof(Block);

  Block blk;
  std::memcpy(&blk, base, sizeof(Block));

  blk.allocated = false;
  std::memcpy(base, &blk, sizeof(Block));
}

} // namespace layer
