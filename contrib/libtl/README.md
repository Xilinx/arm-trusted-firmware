# Transfer List Library (LibTL)

The Transfer List Library (LibTL) implements the [Firmware
Handoff](https://github.com/FirmwareHandoff/firmware_handoff) specification,
providing a streamlined interface for managing transfer lists. LibTL offers a
user-friendly interface for:

- Creating transfer lists
- Reading and extracting data from transfer lists
- Manipulating and updating transfer lists

The library supports building with host tools such as Clang and GCC, and cross
compilation with the Aarch64 GNU compiler.

## Minimum Supporting Tooling Requirements

| Tool         | Minimum Version |
| ------------ | --------------- |
| Clang-Format | 14              |
| CMake        | 3.15            |

## Building with CMake

To configure the project with the default settings (using **GCC** as the host
compiler), run:

```sh
cmake -B build
```

Then, to compile the project and produce `libtl.a` in the `build/` directory:

```sh
cmake --build build
```

To cross-compile the project (e.g., for AArch64), specify the appropriate tool
using the `CC` option when configuring the project:

```sh
CC=aarch64-none-elf-gcc cmake -B build -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
cmake --build build
```

You can specify the build mode using the `CMAKE_BUILD_TYPE` option. Supported
modes include:

- `Debug` – Full debug info with no optimization.
- `Release` – Optimized build without debug info.
- `RelWithDebInfo` – Optimized build with debug info.
- `MinSizeRel` – Optimized for minimum size.

APIs for specific projects can be conditionally included in the static library
using the `PROJECT_API` option.

## Testing with CTest

Tests for LibTL are provided in the folder `test`, to configure the project for
test builds, run:

```sh
cmake -B build -DTARGET_GROUP=test
cmake --build build
```

Then, to run the tests with `ctest`, use:

```sh
ctest --test-dir build/
```
