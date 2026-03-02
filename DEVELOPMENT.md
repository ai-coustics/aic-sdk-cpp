# Running Tests

```bash
export AIC_SDK_LICENSE="<your-license-key>"
cmake -B build ./test
cmake --build build -j
cd build && ctest --output-on-failure
```

# Release Steps

1. Update CMake project version in `CMakeLists.txt`
2. Update the `checksum.txt` file with the current version
3. Update git tag for FetchContent in `example/CMakeLists.txt`
4. Update git tag in `README.md` CMake integration example
