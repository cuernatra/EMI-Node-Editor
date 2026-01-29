# AO_Emi-Visual-Programming-Tool

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

