---
description: Build projects with specified compiler(s)
---

Build `$ARGUMENTS` in `.temp/build-{compiler}/` using CMake.

- Default: MSVC on Windows, GCC elsewhere
- If gcc/clang/msvc mentioned: use that compiler
- If multiple compilers: build all

Output: `.temp/build-msvc/`, `.temp/build-gcc/`, `.temp/build-clang/`
