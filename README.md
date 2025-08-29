# ⚠️ WARNING

This project is in active development, as such a lot **WILL** be changed. For further information, see **TODO** and **Known issues**.

# libcharm - arm32 static recompiler

`charm` is an effort to create an `arm32` static recompiler, that compiles arm to cross-platform C++ code and can produce output without requiring user to tweak the resulting code.

It does so by emulating arm instructions, implementing virtual addressing routines (that user can override) and creating several methods of overriting already existing functions. The drawback is that output is not human-readable C++ code, but rather something closer to the original assembly.

Please note that this project currently **only supports recompiling `Linux` binaries** and only works with those that are **compiled for `armv4t` (without Thumb)**.

## ❓ What's already done?

- `armv4` instructon set implementation (no Thumb yet).
- Address mapping.
- ELF sections mapping.

## 🗒️ TODO

This is a long-term TODO list, for a short-term one see `TODO.md`!

- Implement `thumb` instruction set.
- Expand the instruction set to `armv5te`.
- Expand `dump` mode functionality.

## ⚠️ Known issues

- One of arithmetic instructions is misbehaving, currently under investigation.

# charm-cli - Command line interface

Please note, before using `recomp`, you have to create `deps` directory in your build dir and copy over liblayer directory from src there

`charm-cli` has two modes of operation: `recomp` (Recompile) and `dump` (Dump/dissasemble).
`dump` is self-explanatory -- it just dumps dissassembly and other information into output file.
.

`recomp`, however is much more complex. It takes an ELF arm32 executable as an input file and generates an entire Makefile project. Note that Makefile is only generated once, as such you are free to change it and recompile to the same output directory. It will only be recreated when you delete it.

`recomp` also has a special flag called minify -- it strips as much as possible to produce the smallest source code.

### Example usage

- `charm-cli dump libtest.so dump.txt`
- `charm-cli recomp libtest.so outdir/`
- `charm-cli recomp --minify libtest.so outdir/`
