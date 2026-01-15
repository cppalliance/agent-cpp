---
description: Run test executables
---

Run `$ARGUMENTS` tests. No args = most recently modified .exe across repos.

- Repos: `capy`, `corosio`, `http`, `beast2`, `asio` → `C:\Users\Vinnie\src\boost\libs\{repo}\`
- Platforms: `gcc`, `clang`, `msvc` (default: msvc)
- Exe: `build-{platform}/test/unit/boost_{repo}_tests.exe` (msvc adds `/Release/`)
- Build if missing: `cmake --build build-{platform} --target boost_{repo}_tests`
