# AO_Emi-Visual-Programming-Tool

[![Build](https://github.com/cuernatra/AO_Emi-Visual-Programming-Tool/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/cuernatra/AO_Emi-Visual-Programming-Tool/actions/workflows/build.yml)
[![Static Analysis](https://github.com/cuernatra/AO_Emi-Visual-Programming-Tool/actions/workflows/static-analysis.yml/badge.svg?branch=main)](https://github.com/cuernatra/AO_Emi-Visual-Programming-Tool/actions/workflows/static-analysis.yml)
[![Tests](https://github.com/cuernatra/AO_Emi-Visual-Programming-Tool/actions/workflows/tests.yml/badge.svg?branch=main)](https://github.com/cuernatra/AO_Emi-Visual-Programming-Tool/actions/workflows/tests.yml)

COMP.SE.610/620

## Build instructions

Make sure CMake is installed.

1. Create build directory:

   ```bash
   mkdir build
   ```

2. Go to build directory:

   ```bash
   cd build
   ```

3. Configure CMake to build dependencies as static libraries:

   ```bash
   cmake -DBUILD_SHARED_LIBS=OFF ..
   ```

4. Build project:

   ```bash
   cmake --build .
   ```

Generated executable can be found in the build directory.

## Testing

Configure with tests enabled:

```bash
cmake -S . -B build -DBUILD_TESTS=ON
```

Build and run tests:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

## Documentation

Generate the HTML docs:

```bash
doxygen Doxyfile
```

Then open `docs/index.html` (redirects to `docs/html/index.html`).

