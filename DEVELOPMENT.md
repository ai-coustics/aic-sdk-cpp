# Release Steps

1. Update CMake project version in `CMakeLists.txt`
2. Update the `checksum.txt` file with the current version
3. Update git tag for FetchContent in `example/CMakeLists.txt`
4. Update git tag for FetchContent in `test/CMakeLists.txt`
5. Update git tag in `README.md` CMake integration example
6. Update `AIC_TEST_MODEL_VERSION` in `test/CMakeLists.txt` and `AIC_MODEL_VERSION` in
   `.github/workflows/test.yml` when the compatible model version changes

# Tests

```sh
export AIC_SDK_LICENSE="your_license_key_here"

cmake -B build-test ./test
cmake --build build-test -j
ctest --test-dir build-test --output-on-failure
```

Audio fixtures must be mono, 32-bit float WAV files using the `WAVE_FORMAT_IEEE_FLOAT` tag
(`0x0003`), not `WAVE_FORMAT_EXTENSIBLE` (`0xfffe`). `AudioFile` 1.1.4 accepts extensible WAVs
but silently decodes their 32-bit IEEE-float payload as signed PCM. The test rejects extensible WAVs
so fixture changes cannot produce a comparison against corrupted samples.

Regenerate the reference output for a model:

```sh
cmake -B build-test ./test -DAIC_TEST_MODEL_ID=<model-id>
cmake --build build-test -j
./build-test/aic-sdk-e2e-test --generate build-test/models/<model-file>.aicmodel \
    test/fixtures/test_signal.wav \
    test/fixtures/<model-id>/test_signal_enhanced.wav
```
