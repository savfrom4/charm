# ⚠️ WARNING

This is a side project I've been working on in my free time. It's NOT MEANT to be used in production anytime soon, with each iteration changing interface drastically.

# charm - 32-bit ARM static recompiler to C++

`charm` is my effort to create a ARM-to-C++ "low-effort" static recompiler, with "low-effort" meaning little to no tweaks to the generated code. It does not produce human-readable code (like ghidra, or IDA), rather it outputs a bunch of emulated instruction calls. In a sense, it's an emulator with fetch/decode steps skipped. In theory, such AOT approach should improve the perfomance greatly, even when compared to JIT recompilation, with tradeoff being human readability.

## ❓ What's already done?

- Base `armv4` instructon set reimplementation.
- Address mapping.
- ELF sections mapping.

## 🗒️ TODO

This is my a long-term TODO list!

- Implement `thumb` instruction set.
- Expand the instruction set support to `armv5te`.
- Expand `dump` mode functionality.

# charm - Command line recompiler interfaces

`charm` has two modes of operation: `recomp` (Recompile) and `dump` (Dump/dissasemble).
`dump` is self-explanatory -- it just dumps dissassembly and other information into output file.
.

`recomp`, however is much more complex. It takes an ELF arm executable as an input file and generates code, address mappings, section mappings and bundles it all as `meson` project. Note that meson-related files are only generated once, as such you are free to change them and recompile to the same output directory.

`recomp` also has a special flag called minify -- it strips as much as possible to produce the smallest source code.

### Example usage

- `charm-cli dump libtest.so dump.txt`
- `charm-cli recomp libtest.so outdir/`
- `charm-cli recomp --minify libtest.so outdir/`

# charm-dbg

`charm-dbg` is an internal debugger used by the project. It operates over TCP and allows remote control and debugging of a recompiled executable (of course, if it was compiled with `LIBLAYER_DEBUG` set)
