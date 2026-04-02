# AO_Emi-Visual-Programming-Tool

[![CI](https://github.com/cuernatra/AO_Emi-Visual-Programming-Tool/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/cuernatra/AO_Emi-Visual-Programming-Tool/actions/workflows/ci.yml)

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

## Documentation

Generate the HTML docs:

```bash
doxygen Doxyfile
```

Then open `docs/index.html` (redirects to `docs/html/index.html`).

