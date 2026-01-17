# CMake Mastery: A Deep Tutorial for Windows Development

## Part 1: Understanding What CMake Actually Is

### The Fundamental Problem CMake Solves

Before CMake, building C++ projects across platforms was a nightmare. Unix systems used Makefiles, Windows used Visual Studio project files, macOS used Xcode projects. Each had completely different syntax, capabilities, and quirks. If you wanted your project to build on all platforms, you had to maintain three separate build systems—a recipe for bugs, drift, and madness.

CMake is a **build system generator**, not a build system itself. This distinction is crucial:

- **Build system** (Make, Ninja, MSBuild): Takes build instructions and invokes compilers/linkers
- **Build system generator** (CMake): Takes a platform-agnostic description and *produces* build system instructions

When you run CMake, it reads your `CMakeLists.txt` files and generates native build files for whatever platform you're on. On Windows, that might be Visual Studio solution files (`.sln`) or Ninja build files. On Linux, typically Makefiles or Ninja. The actual compilation is done by those native tools, not by CMake.

### The Two-Phase Architecture

CMake operates in two distinct phases, and understanding this is key to debugging issues:

**Phase 1: Configure**

During configuration, CMake:

1. Reads your `CMakeLists.txt` files top-to-bottom
2. Discovers your compiler, platform, and system libraries
3. Evaluates all CMake logic (conditionals, loops, function calls)
4. Populates the **CMake cache** with discovered and user-specified values
5. Validates that everything makes sense

The output of configuration is internal CMake state plus the `CMakeCache.txt` file.

**Phase 2: Generate**

During generation, CMake:

1. Takes the configured state
2. Produces native build system files (e.g., `build.ninja`, `*.vcxproj`)
3. Writes these to your build directory

On the command line, both phases happen together:

```bash
cmake -S . -B build  # Configure AND generate
```

In CMake-GUI, they're separate buttons: "Configure" then "Generate."

**Why this matters:** If your project won't configure, the problem is in your `CMakeLists.txt` logic or missing dependencies. If it configures but won't generate, there's likely a generator-specific issue. If it generates but won't build, the problem is in your actual code or compiler settings.

### Source Trees vs. Build Trees

CMake enforces separation between where your source code lives and where build artifacts go:

- **Source tree**: Your project directory containing `CMakeLists.txt` and source files
- **Build tree** (binary directory): Where CMake generates build files and where compiled artifacts land

**Never do in-source builds.** Always use a separate build directory:

```bash
# Good: out-of-source build
cmake -S . -B build

# Avoid: in-source build (pollutes your source tree)
cmake .
```

Key variables reflecting these directories:

| Variable | Meaning |
|----------|---------|
| `CMAKE_SOURCE_DIR` | Top-level source directory (where root `CMakeLists.txt` lives) |
| `CMAKE_BINARY_DIR` | Top-level build directory |
| `CMAKE_CURRENT_SOURCE_DIR` | Source directory currently being processed |
| `CMAKE_CURRENT_BINARY_DIR` | Build directory corresponding to current source directory |

---

## Part 2: Generators—The Heart of Platform Abstraction

A **generator** tells CMake what kind of native build files to produce. The generator choice fundamentally affects how you interact with your build.

### Single-Configuration vs. Multi-Configuration Generators

This is one of the most confusing aspects of CMake, especially on Windows where both types are common.

**Single-configuration generators** (Makefiles, Ninja):

- You choose Debug or Release at *configure* time
- The entire build tree is locked to that one configuration
- Want both Debug and Release? You need two separate build directories

```bash
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
```

**Multi-configuration generators** (Visual Studio, Xcode, Ninja Multi-Config):

- Configuration is chosen at *build* time
- One build tree can produce Debug, Release, RelWithDebInfo, MinSizeRel
- `CMAKE_BUILD_TYPE` is ignored; use `--config` when building

```bash
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
cmake --build build --config Release
```

### Common Windows Generators

| Generator | Type | Notes |
|-----------|------|-------|
| `Visual Studio 17 2022` | Multi-config | Produces `.sln`/`.vcxproj` files |
| `Visual Studio 16 2019` | Multi-config | For VS2019 |
| `Ninja` | Single-config | Fast, requires MSVC environment setup |
| `Ninja Multi-Config` | Multi-config | Best of both worlds |
| `NMake Makefiles` | Single-config | Uses Microsoft's nmake |

### The Visual Studio Generator Environment Problem

The Visual Studio generator is self-contained—it finds MSVC automatically because VS solution files know how to locate the compiler.

But **Ninja and NMake generators require you to run from a "Developer Command Prompt"** or explicitly set up the MSVC environment. This is a major source of Windows CMake headaches.

The MSVC environment is set up by `vcvarsall.bat`, which configures `PATH`, `INCLUDE`, `LIB`, and other variables so the command line can find `cl.exe` (the compiler), `link.exe` (the linker), and Windows SDK headers/libraries.

```batch
:: Option 1: Use Developer Command Prompt (already runs vcvarsall.bat)

:: Option 2: Manually invoke vcvarsall.bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

:: Then CMake can find MSVC
cmake -S . -B build -G Ninja
```

The `x64` argument specifies the target architecture. Other options: `x86`, `arm64`, `x86_amd64` (cross-compile to x64 from x86).

---

## Part 3: The CMake Cache—Your Build's Memory

The cache (`CMakeCache.txt`) is a key-value store that persists between CMake runs. Understanding it eliminates much confusion.

### What Goes in the Cache

1. **User settings**: Things you set with `-D` on the command line
2. **Discovery results**: Compiler paths, library locations, feature detection results
3. **Project options**: Values from `option()` and cached `set()` calls

### Cache Variable Syntax

```cmake
set(MY_OPTION "default_value" CACHE STRING "Description for GUI tools")
```

The type (STRING, BOOL, PATH, FILEPATH) is mainly for GUI tools like `cmake-gui` or `ccmake`.

### Cache Behavior That Surprises People

**Values persist:** Once a variable is in the cache, CMake won't overwrite it from `CMakeLists.txt`. This is intentional—it lets users override defaults without having their changes clobbered.

```cmake
# This ONLY sets MY_VAR if it's not already in the cache
set(MY_VAR "default" CACHE STRING "")
```

**Forcing a value:**

```cmake
# This ALWAYS sets MY_VAR, even if cached
set(MY_VAR "forced" CACHE STRING "" FORCE)
```

**Command-line values win:** `-D` values are applied before `CMakeLists.txt` runs but take precedence over non-FORCE cache sets.

### Common Cache Operations

```bash
# Set a cache variable
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Delete a cache variable
cmake -S . -B build -U CMAKE_CXX_FLAGS

# Delete entire cache and reconfigure
cmake -S . -B build --fresh

# Or manually delete and reconfigure
rm -rf build/CMakeCache.txt build/CMakeFiles
cmake -S . -B build
```

### Inspecting the Cache

```bash
# List all non-advanced cache variables
cmake -S . -B build -L

# Include advanced variables
cmake -S . -B build -LA

# Include help strings
cmake -S . -B build -LAH
```

Or just open `build/CMakeCache.txt` in a text editor—it's human-readable.

---

## Part 4: Targets—The Modern CMake Mental Model

Pre-2014 CMake (before version 3.0) relied heavily on global variables to set compiler flags, include paths, and definitions. This was fragile and didn't compose well.

Modern CMake is **target-centric**. Everything revolves around targets and their properties.

### What Is a Target?

A target is something CMake knows how to build or something that carries build information:

- **Executable targets**: Created by `add_executable()`
- **Library targets**: Created by `add_library()` (STATIC, SHARED, MODULE, OBJECT, INTERFACE)
- **Imported targets**: Pre-built libraries you're consuming
- **Alias targets**: Alternative names for existing targets

### Target Properties

Every target has properties that control how it's built:

- `INCLUDE_DIRECTORIES`: Where to find headers
- `COMPILE_DEFINITIONS`: Preprocessor defines
- `COMPILE_OPTIONS`: Compiler flags
- `LINK_LIBRARIES`: What to link against
- `CXX_STANDARD`: Which C++ standard to use

### The `target_*` Commands

```cmake
add_library(mylib STATIC mylib.cpp)

target_include_directories(mylib PUBLIC include)
target_compile_definitions(mylib PRIVATE MYLIB_INTERNAL)
target_compile_features(mylib PUBLIC cxx_std_20)
target_link_libraries(mylib PRIVATE some_dependency)
```

### PUBLIC, PRIVATE, and INTERFACE

This is where many people get confused. These keywords control **property propagation**:

| Keyword | Affects building this target? | Propagates to dependents? |
|---------|-------------------------------|---------------------------|
| `PRIVATE` | Yes | No |
| `PUBLIC` | Yes | Yes |
| `INTERFACE` | No | Yes |

**Example:**

```cmake
add_library(mylib STATIC mylib.cpp)
target_include_directories(mylib PUBLIC include)   # Both mylib and its consumers see this
target_include_directories(mylib PRIVATE src)      # Only mylib sees this
target_compile_definitions(mylib INTERFACE USE_MYLIB)  # Only consumers see this

add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE mylib)
# myapp automatically gets the PUBLIC include directory and INTERFACE definition
```

**Rule of thumb:**

- Headers your consumers need: `PUBLIC`
- Implementation details: `PRIVATE`
- Header-only library requirements: `INTERFACE`

### Interface Libraries

An `INTERFACE` library has no source files—it only carries properties:

```cmake
add_library(header_only INTERFACE)
target_include_directories(header_only INTERFACE include)
target_compile_features(header_only INTERFACE cxx_std_17)
```

These are perfect for header-only libraries and for collecting common settings.

---

## Part 5: CMake Presets—Taming Configuration Complexity

Before presets, sharing build configurations required README documentation, scripts, and tribal knowledge. CMake presets standardize this.

### The Preset Files

- `CMakePresets.json`: Project-wide presets, checked into version control
- `CMakeUserPresets.json`: Personal presets, not checked in (add to `.gitignore`)

### Preset Structure

```json
{
  "version": 6,
  "cmakeMinimumRequired": { "major": 3, "minor": 23, "patch": 0 },
  "configurePresets": [...],
  "buildPresets": [...],
  "testPresets": [...]
}
```

### Configure Presets

Define how to run the configure step:

```json
{
  "configurePresets": [
    {
      "name": "windows-msvc-debug",
      "displayName": "Windows MSVC Debug",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/${presetName}",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_CXX_STANDARD": "20"
      },
      "toolset": {
        "value": "v143",
        "strategy": "external"
      },
      "architecture": {
        "value": "x64",
        "strategy": "external"
      }
    }
  ]
}
```

### Build Presets

Define how to run the build step:

```json
{
  "buildPresets": [
    {
      "name": "windows-msvc-debug",
      "configurePreset": "windows-msvc-debug",
      "configuration": "Debug"
    }
  ]
}
```

### Test Presets

Define how to run tests:

```json
{
  "testPresets": [
    {
      "name": "windows-msvc-debug",
      "configurePreset": "windows-msvc-debug",
      "configuration": "Debug",
      "output": { "outputOnFailure": true }
    }
  ]
}
```

### Using Presets

```bash
# List available presets
cmake --list-presets

# Configure with a preset
cmake --preset windows-msvc-debug

# Build with a preset
cmake --build --preset windows-msvc-debug

# Test with a preset
ctest --preset windows-msvc-debug
```

### Preset Inheritance

Presets can inherit from others to avoid repetition:

```json
{
  "configurePresets": [
    {
      "name": "base",
      "hidden": true,
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/${presetName}",
      "cacheVariables": {
        "CMAKE_CXX_STANDARD": "20"
      }
    },
    {
      "name": "debug",
      "inherits": "base",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug"
      }
    },
    {
      "name": "release",
      "inherits": "base",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release"
      }
    }
  ]
}
```

### Presets and Visual Studio / Cursor / VSCode

IDEs that support CMake presets will:

1. Detect `CMakePresets.json` automatically
2. Show presets in a dropdown instead of requiring manual kit selection
3. Use preset settings for IntelliSense configuration

In **VSCode/Cursor with CMake Tools extension**:

- Set `"cmake.useCMakePresets": "always"` in settings
- The extension reads presets and presents them in the status bar
- You select a configure preset, then a build preset

### The MSVC Environment Problem with Presets

For Ninja presets with MSVC, you need the compiler environment. The `architecture` and `toolset` fields with `"strategy": "external"` tell the IDE to source the environment for you.

For command-line usage outside an IDE, you still need to run from a Developer Command Prompt or call `vcvarsall.bat` first.

---

## Part 6: Testing with CTest

CTest is CMake's testing driver. It doesn't care what test framework you use—it just runs executables and checks exit codes.

### Enabling Testing

In your root `CMakeLists.txt`:

```cmake
enable_testing()
```

Or use the CTest module (adds some extra dashboard features):

```cmake
include(CTest)  # Calls enable_testing() and creates BUILD_TESTING option
```

### Adding Tests

```cmake
add_executable(my_test test_main.cpp)
add_test(NAME my_test COMMAND my_test)
```

The test passes if the executable returns 0, fails otherwise.

### Test Properties

```cmake
add_test(NAME slow_test COMMAND slow_test)
set_tests_properties(slow_test PROPERTIES
  TIMEOUT 300                    # Fail if takes > 5 minutes
  LABELS "slow;integration"      # For filtering
  ENVIRONMENT "MY_VAR=value"     # Set environment for this test
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test_data
)
```

### Running Tests

```bash
# Run all tests
ctest --test-dir build

# Run with output on failure
ctest --test-dir build --output-on-failure

# Run tests matching a regex
ctest --test-dir build -R "unit_.*"

# Exclude tests matching a regex
ctest --test-dir build -E "slow_.*"

# Run tests with specific labels
ctest --test-dir build -L unit

# Run in parallel
ctest --test-dir build -j8

# Verbose output
ctest --test-dir build -V

# For multi-config generators, specify configuration
ctest --test-dir build -C Debug
```

### Test Presets

Define test presets in `CMakePresets.json`:

```json
{
  "testPresets": [
    {
      "name": "unit-tests",
      "configurePreset": "debug",
      "configuration": "Debug",
      "filter": {
        "include": { "label": "unit" }
      },
      "output": {
        "outputOnFailure": true,
        "verbosity": "default"
      },
      "execution": {
        "jobs": 8,
        "stopOnFailure": false
      }
    }
  ]
}
```

---

## Part 7: Controlling Output Locations

By default, executables and libraries end up scattered through your build tree mirroring the source tree structure. You can centralize them.

### Per-Target Output Directories

```cmake
add_executable(myapp main.cpp)
set_target_properties(myapp PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
  ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
```

### Global Output Directories

Set these before defining targets:

```cmake
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
```

### Multi-Config Generators

With Visual Studio or Ninja Multi-Config, you often want per-config subdirectories:

```cmake
# For multi-config generators, outputs go to bin/Debug, bin/Release, etc.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# If you want flat directories (no config subdirs):
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
```

---

## Part 8: Complete Windows Workflow Examples

### Example 1: Simple Project with Ninja

```bash
# Open Developer Command Prompt for VS 2022

# Configure
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build

# Test
ctest --test-dir build
```

### Example 2: Using Visual Studio Generator

```bash
# Regular command prompt is fine—VS generator finds MSVC automatically

# Configure (creates .sln)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# Build Debug
cmake --build build --config Debug

# Build Release
cmake --build build --config Release

# Test Debug
ctest --test-dir build -C Debug
```

### Example 3: Full Preset Workflow

`CMakePresets.json`:

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "msvc-base",
      "hidden": true,
      "generator": "Ninja Multi-Config",
      "binaryDir": "${sourceDir}/build",
      "cacheVariables": {
        "CMAKE_CXX_STANDARD": "20"
      },
      "architecture": { "value": "x64", "strategy": "external" },
      "toolset": { "value": "v143", "strategy": "external" }
    },
    {
      "name": "default",
      "inherits": "msvc-base",
      "displayName": "Default (MSVC Ninja Multi-Config)"
    }
  ],
  "buildPresets": [
    {
      "name": "debug",
      "configurePreset": "default",
      "configuration": "Debug"
    },
    {
      "name": "release",
      "configurePreset": "default",
      "configuration": "Release"
    }
  ],
  "testPresets": [
    {
      "name": "debug",
      "configurePreset": "default",
      "configuration": "Debug",
      "output": { "outputOnFailure": true }
    }
  ]
}
```

Usage:

```bash
# From Developer Command Prompt
cmake --preset default
cmake --build --preset debug
ctest --preset debug
```

---

## Part 9: Debugging CMake Problems

### "No CMAKE_CXX_COMPILER could be found"

**Cause:** CMake can't find a compiler.

**Solutions:**

1. For Visual Studio generator: Make sure VS is installed with C++ workload
2. For Ninja/NMake: Run from Developer Command Prompt
3. Explicitly set compiler: `-DCMAKE_CXX_COMPILER=cl.exe`

### "The source directory does not contain a CMakeLists.txt file"

**Cause:** Wrong path to source directory.

**Solution:** Check your `-S` path points to the directory containing `CMakeLists.txt`.

### Cache Seems Stuck on Wrong Values

**Cause:** Cache persists values between runs.

**Solution:**

```bash
cmake --fresh -S . -B build   # CMake 3.24+
# or
rm -rf build/CMakeCache.txt build/CMakeFiles
cmake -S . -B build
```

### "Could not find package X"

**Cause:** CMake can't locate a dependency.

**Solutions:**

1. Set `CMAKE_PREFIX_PATH` to where the package is installed
2. Set `<Package>_DIR` to the directory containing its config file
3. Make sure the package is actually installed

### Visual Studio Project Is Out of Sync

**Cause:** CMake files changed but VS hasn't reconfigured.

**Solution:** In VS, right-click `CMakeLists.txt` → "Delete Cache and Reconfigure"

### Build Works in IDE but Fails on Command Line

**Cause:** IDE sets up environment that command line lacks.

**Solution:** Use Developer Command Prompt, or ensure preset has proper toolset/architecture settings.

---

## Part 10: Best Practices Summary

### Do

- **Use targets**, not global variables for properties
- **Use out-of-source builds** (`cmake -S . -B build`)
- **Use presets** for reproducible configurations
- **Specify minimum CMake version** with required features
- **Use `target_link_libraries`** for dependencies (it handles everything)
- **Use generator expressions** for config-specific settings: `$<CONFIG:Debug>`

### Don't

- **Don't use** `include_directories()`, `add_definitions()`, `link_libraries()` (global scope)
- **Don't hardcode paths**—use CMake variables and find_package
- **Don't edit CMakeCache.txt directly** if you can help it
- **Don't assume single-config**—support multi-config generators
- **Don't rely on `CMAKE_BUILD_TYPE`** with multi-config generators

### Project Structure Recommendation

```
MyProject/
├── CMakeLists.txt
├── CMakePresets.json
├── src/
│   ├── CMakeLists.txt
│   └── ...
├── include/
│   └── myproject/
│       └── ...
├── tests/
│   ├── CMakeLists.txt
│   └── ...
├── extern/           # Third-party dependencies
│   └── ...
└── build/            # Generated, not in version control
```

Root `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.23)
project(MyProject VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
```

---

## Part 11: Quick Reference

### Essential Commands

| Command | Purpose |
|---------|---------|
| `cmake -S <src> -B <build>` | Configure |
| `cmake --build <build>` | Build |
| `cmake --build <build> --config <cfg>` | Build specific config |
| `cmake --install <build>` | Install |
| `ctest --test-dir <build>` | Run tests |
| `cmake --preset <name>` | Configure with preset |
| `cmake --build --preset <name>` | Build with preset |
| `ctest --preset <name>` | Test with preset |

### Essential Variables

| Variable | Purpose |
|----------|---------|
| `CMAKE_BUILD_TYPE` | Debug, Release, etc. (single-config only) |
| `CMAKE_CXX_STANDARD` | C++ standard version |
| `CMAKE_PREFIX_PATH` | Where to find packages |
| `CMAKE_INSTALL_PREFIX` | Where to install |
| `CMAKE_TOOLCHAIN_FILE` | Cross-compilation toolchain |

### Essential Commands in CMakeLists.txt

| CMake Command | Purpose |
|---------------|---------|
| `cmake_minimum_required(VERSION x.y)` | Require CMake version |
| `project(Name VERSION x.y LANGUAGES CXX)` | Declare project |
| `add_executable(name sources...)` | Create executable target |
| `add_library(name [STATIC|SHARED|INTERFACE] sources...)` | Create library target |
| `target_include_directories(target SCOPE dirs...)` | Add include paths |
| `target_link_libraries(target SCOPE libs...)` | Link dependencies |
| `target_compile_features(target SCOPE features...)` | Require compiler features |
| `target_compile_definitions(target SCOPE defs...)` | Add preprocessor defines |
| `find_package(Name REQUIRED)` | Find external dependency |
| `enable_testing()` | Enable CTest |
| `add_test(NAME name COMMAND cmd)` | Add a test |

---

## Part 12: Cursor IDE—Special Considerations for C++ and CMake

Cursor is a VS Code fork with AI capabilities. While it inherits VS Code's CMake Tools extension support, there are critical differences that affect C++ development on Windows.

### The Microsoft Extension Block (April 2025)

As of April 2025, **Microsoft's C/C++ extension no longer works in Cursor**. Microsoft added an environment check in v1.24.5 that blocks non-Microsoft editors. This means:

- The familiar `c_cpp_properties.json` configuration **does not work**
- Microsoft IntelliSense is unavailable
- The `ms-vscode.cpptools` extension will display a licensing error

**Cursor's solution:** Anysphere (Cursor's developer) created replacement extensions that bundle open-source alternatives.

### The Anysphere C/C++ Extension

Install the **C/C++ extension from Anysphere** (not Microsoft). This extension uses:

- **clangd** for language server (IntelliSense, go-to-definition, etc.)
- **CodeLLDB** for debugging (instead of Microsoft's cppvsdbg)
- **CMake Tools** (bundled automatically)

**Critical difference:** Clangd requires a `compile_commands.json` file to understand your project. It does **not** read `c_cpp_properties.json`.

### Generating compile_commands.json

This is where CMake generator choice becomes crucial for Cursor users:

| Generator | Produces compile_commands.json? |
|-----------|--------------------------------|
| Ninja | ✅ Yes |
| Unix Makefiles | ✅ Yes |
| NMake Makefiles | ✅ Yes |
| Visual Studio | ❌ **No** |

**If you use the Visual Studio generator, clangd won't work properly in Cursor.**

To generate `compile_commands.json`, either:

1. **Use Ninja generator:**

```bash
cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

2. **Or set the variable globally in your CMakeLists.txt:**

```cmake
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

3. **Or add to CMakePresets.json:**

```json
{
  "name": "cursor-friendly",
  "generator": "Ninja",
  "cacheVariables": {
    "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
  }
}
```

The generated `compile_commands.json` will be in your build directory. Clangd finds it automatically if it's in the build directory or project root.

### Symlinking compile_commands.json (Optional)

Some setups work better with the file in the project root:

```bash
# PowerShell (as admin) or use mklink in cmd
New-Item -ItemType SymbolicLink -Path "compile_commands.json" -Target "build/compile_commands.json"
```

Or configure clangd to look in the build directory by creating `.clangd` in your project root:

```yaml
CompileFlags:
  CompilationDatabase: build
```

### The CMAKE_ROOT Terminal Bug

Cursor's integrated terminal has a known bug where CMake can't find CMAKE_ROOT:

```
CMake Error: Could not find CMAKE_ROOT !!!
CMake has most likely not been installed correctly.
```

This affects zsh primarily and has persisted for over a year.

**Workarounds:**

1. **Switch to bash in the terminal:**
   ```bash
   bash
   cmake --version  # Now works
   ```

2. **Start a new shell instance:**
   ```bash
   zsh  # or bash
   ```

3. **Use an external terminal** for CMake commands

4. **Use CMake Tools extension commands** (Ctrl+Shift+P → "CMake: Configure") instead of terminal

### Recommended Cursor + CMake + Windows Setup

**Step 1: Install Extensions**

- Anysphere C/C++ (includes clangd, CodeLLDB, CMake Tools)
- Do NOT install Microsoft C/C++

**Step 2: Use Ninja Generator**

Create `CMakePresets.json`:

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "windows-base",
      "hidden": true,
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/${presetName}",
      "cacheVariables": {
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
      }
    },
    {
      "name": "debug",
      "inherits": "windows-base",
      "displayName": "Debug",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug"
      }
    },
    {
      "name": "release",
      "inherits": "windows-base",
      "displayName": "Release",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "debug",
      "configurePreset": "debug"
    },
    {
      "name": "release",
      "configurePreset": "release"
    }
  ]
}
```

**Step 3: Launch Cursor Properly**

Since Ninja requires MSVC environment, either:

- **Option A:** Launch Cursor from Developer Command Prompt:
  ```batch
  "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
  cursor .
  ```

- **Option B:** Create a batch file shortcut:
  ```batch
  @echo off
  call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
  start "" "C:\Users\%USERNAME%\AppData\Local\Programs\cursor\Cursor.exe" %*
  ```

- **Option C:** Use CMake Tools' kit scanning (Ctrl+Shift+P → "CMake: Scan for Compilers")

**Step 4: Configure clangd (Optional)**

Create `.clangd` in project root for custom settings:

```yaml
CompileFlags:
  CompilationDatabase: build/debug
  Add:
    - "-Wno-unknown-warning-option"

Diagnostics:
  UnusedIncludes: None

InlayHints:
  Enabled: true
  ParameterNames: true
  DeducedTypes: true
```

### Debugging in Cursor

The Anysphere extension uses CodeLLDB instead of Microsoft's debugger. Create `.vscode/launch.json`:

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "type": "lldb",
      "request": "launch",
      "name": "Debug",
      "program": "${command:cmake.launchTargetPath}",
      "args": [],
      "cwd": "${workspaceFolder}",
      "preLaunchTask": "CMake: build"
    }
  ]
}
```

**Note:** LLDB works well for most debugging but has some differences from MSVC's debugger in how it displays certain Windows-specific types.

### Known Cursor + CMake Issues Summary

| Issue | Cause | Solution |
|-------|-------|----------|
| MS C/C++ extension blocked | License enforcement since April 2025 | Use Anysphere C/C++ extension |
| IntelliSense not working | Missing compile_commands.json | Use Ninja generator with CMAKE_EXPORT_COMPILE_COMMANDS=ON |
| c_cpp_properties.json ignored | Clangd doesn't use it | Create compile_commands.json instead |
| CMAKE_ROOT not found in terminal | Cursor terminal environment bug | Use bash instead of zsh, or external terminal |
| Include paths not resolved | VS generator doesn't produce compile_commands.json | Switch to Ninja generator |
| Formatting not working | clangd uses .clang-format, not VS settings | Create .clang-format file |

### When to Use Visual Studio Instead

If you absolutely need the Visual Studio generator (for solution files, MSBuild integration, or specific VS features), consider:

1. Using Visual Studio itself for development
2. Using Cursor only for AI assistance and editing, but building from command line
3. Generating a compile_commands.json separately using tools like `compdb` or `bear`

### Migrating from VS Code to Cursor

If your project worked in VS Code with Microsoft C/C++:

1. Remove/disable Microsoft C/C++ extension settings from `.vscode/c_cpp_properties.json`
2. Add `CMAKE_EXPORT_COMPILE_COMMANDS=ON` to your CMake configuration
3. Switch generator to Ninja if using Visual Studio generator
4. Reconfigure and rebuild
5. Restart Cursor to let clangd index the project

---

This should give you the foundation to understand and control CMake on Windows. The key insights are:

1. CMake generates build files, it doesn't compile
2. Generators determine what kind of build files
3. Single-config vs multi-config changes everything about how you specify Debug/Release
4. The cache persists settings between runs
5. Modern CMake is target-centric—use `target_*` commands
6. Presets standardize and share configurations
7. On Windows with Ninja, you need the MSVC environment set up