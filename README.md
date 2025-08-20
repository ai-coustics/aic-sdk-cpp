# ai-coustics Speech Enhancement SDK for C++

A modern C++ wrapper around the ai-coustics Speech Enhancement SDK, providing type-safe enums and RAII resource management while maintaining a thin layer over the underlying C API.

## 🎯 What is this SDK?

Our Speech Enhancement SDK delivers state-of-the-art audio processing capabilities, enabling you to enhance speech clarity and intelligibility in real-time.

## 🚀 Quick Start

### Acquire an SDK License Key

To use the SDK, you'll need a license key. Contact our team to receive your time-limited demo key:

- **Email**: [info@ai-coustics.com](mailto:info@ai-coustics.com)
- **Website**: [ai-coustics.com](https://ai-coustics.com)

Once you have your license key, set it as an environment variable or pass it directly to the SDK initialization functions.

### CMake Integration

Whith this CMake example you can add the SDK to your application and let CMake download all necessary files automatically:

```cmake
include(FetchContent)

set(AIC_SDK_ALLOW_DOWNLOAD ON CACHE BOOL "Allow C SDK download at configure time")

FetchContent_Declare(
        aic_sdk
        GIT_REPOSITORY https://github.com/ai-coustics/aic-sdk-cpp.git
        GIT_TAG        0.6.2
        GIT_SHALLOW    TRUE
    )
FetchContent_MakeAvailable(aic_sdk)

target_link_libraries(my_app PRIVATE aic-sdk)
```

### Example Usage

Here's a simple example showing the core SDK workflow. Error handling is omitted for clarity - see the [complete example](example/main.cpp) for production-ready code:

```cpp
#include "aic.hpp"

#include <vector>
#include <cstdlib>

int main() {
    // Load your license key
    const char* license = std::getenv("AIC_SDK_LICENSE");

    // Create a new model
    auto creation_result = aic::AicModel::create(aic::ModelType::Quail_S48, license);
    std::unique_ptr<aic::AicModel>& model = creation_result.first;

    // Initialize with your audio settings
    model->initialize(48000, 2, 480);

    // Configure enhancement parameters
    model->set_parameter(aic::Parameter::EnhancementLevel, 0.7f);

    // Process audio (interleaved version available as well)
    std::vector<float> audio_buffer_left(480, 0.0f);
    std::vector<float> audio_buffer_right(480, 0.0f);
    std::vector<float*> audio_buffer_planar = {audio_buffer_left.data(), audio_buffer_right.data()};

    model->process_planar(audio_buffer_planar.data(), 2, 480);

    return 0;
}
```

## 🏃 Running the Example

To run the example, make sure you have set your license key as an environment variable:

```bash
export AIC_SDK_LICENSE="your_license_key_here"
```

Then use the following commands to configure, build and run the example:

```sh
cmake -B build ./example
cmake --build build -j
./build/my_app
```

## 🆘 Support & Resources

### Documentation
- **[Basic Example](example/main.cpp)** - Sample code and integration patterns
- **[CMake Integration Guide](example/CMakeLists.txt)** - Build configuration and SDK integration

### Looking for Other Languages?
The ai-coustics Speech Enhancement SDK is available in multiple programming languages to fit your development needs:

| Language | Repository | Description |
|----------|------------|-------------|
| C | [aic-sdk-c](https://github.com/ai-coustics/aic-sdk-c) | Core C interface and foundation library |
| Node.js | [aic-sdk-node](https://github.com/ai-coustics/aic-sdk-node) | JavaScript/TypeScript bindings for Node.js |
| Python | [aic-sdk-py](https://github.com/ai-coustics/aic-sdk-py) | Pythonic interface |
| Rust | [aic-sdk-rs](https://github.com/ai-coustics/aic-sdk-rs) | Safe Rust Bindings |
| WebAssembly | [aic-sdk-wasm](https://github.com/ai-coustics/aic-sdk-wasm) | Browser-compatible WebAssembly build |

All SDKs provide the same core functionality with language-specific optimizations and idioms.

### Demo Plugin
Experience our speech enhancement models firsthand with our [Demo Plugin](https://github.com/ai-coustics/aic-sdk-plugin) - a complete audio plugin that showcases all available models while serving as a comprehensive C++ integration example.

### 💬 Get Help
Need assistance? We're here to support you:
- **Issues**: [GitHub Issues](https://github.com/ai-coustics/aic-sdk-cpp/issues)
- **Technical Support**: [info@ai-coustics.com](mailto:info@ai-coustics.com)

---

Made with ❤️ by the ai-coustics team
