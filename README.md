# ai-coustics Speech Enhancement SDK for C++

A modern C++ wrapper around the ai-coustics Speech Enhancement SDK, providing type-safe enums and RAII resource management while maintaining a thin layer over the underlying C API.

## What is this SDK?

Our Speech Enhancement SDK delivers state-of-the-art audio processing capabilities, enabling you to enhance speech clarity and intelligibility in real-time.

## Quick Start

### Generate your SDK License Key

To use this SDK, you'll need to generate an **SDK license key** from our [Development Portal](https://developers.ai-coustics.io).

**Please note:** The SDK license key is different from our cloud API product. If you have an API license key for our cloud services, it won't work with the SDK - you'll need to create a separate SDK license key in the portal.

### CMake Integration

Whith this CMake example you can add the SDK to your application and let CMake download all necessary files automatically:

```cmake
include(FetchContent)

set(AIC_SDK_ALLOW_DOWNLOAD ON CACHE BOOL "Allow C SDK download at configure time")

FetchContent_Declare(
        aic_sdk
        GIT_REPOSITORY https://github.com/ai-coustics/aic-sdk-cpp.git
        GIT_TAG        0.13.0
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
#include <utility>

int main() {
    // Load your license key
    const char* license = std::getenv("AIC_SDK_LICENSE");

    // Create a new model from a file path
    aic::Result<aic::Model> model_result = aic::Model::create_from_file("/path/to/model.aicmodel");
    aic::Model model = std::move(model_result.value);

    // Create a processor and initialize with your audio settings
    auto processor_result = aic::Processor::create(model, license);
    aic::Processor processor = std::move(processor_result.value);
    aic::ProcessorConfig config = aic::ProcessorConfig::optimal(model).with_num_channels(2);
    processor->initialize(config.sample_rate, config.num_channels, config.num_frames,
                          config.allow_variable_frames);

    // Configure enhancement parameters via a processor context
    auto ctx_result = processor.create_context();
    aic::ProcessorContext ctx = std::move(ctx_result.value);
    ctx.set_parameter(aic::ProcessorParameter::EnhancementLevel, 0.7f);

    // Process audio (interleaved version available as well)
    std::vector<float> audio_buffer_left(config.num_frames, 0.0f);
    std::vector<float> audio_buffer_right(config.num_frames, 0.0f);
    std::vector<float*> audio_buffer_planar = {audio_buffer_left.data(), audio_buffer_right.data()};

    processor.process_planar(audio_buffer_planar.data(), 2, config.num_frames);

    return 0;
}
```

### Compatibility

The wrapper is fully C++11 compatible and on Linux you will need at least GLIBC 2.27 / Ubuntu 18.04.

## Running the Example

To run the example, make sure you have set your license key and model path as environment variables:

```bash
export AIC_SDK_LICENSE="your_license_key_here"
export AIC_SDK_MODEL_PATH="/path/to/model.aicmodel"
```

Then use the following commands to configure, build and run the example (you can also pass the model path as the first argument):

```sh
cmake -B build ./example
cmake --build build -j
./build/my_app
```

## Support & Resources

### Documentation
- **[Basic Example](example/main.cpp)** - Sample code and integration patterns
- **[CMake Integration Guide](example/CMakeLists.txt)** - Build configuration and SDK integration

### Looking for Other Languages?
The ai-coustics Speech Enhancement SDK is available in multiple programming languages to fit your development needs:

| Platform | Repository | Description |
|----------|------------|-------------|
| **C** | [`aic-sdk-c`](https://github.com/ai-coustics/aic-sdk-c) | Core C-Interface |
| **Python** | [`aic-sdk-py`](https://github.com/ai-coustics/aic-sdk-py) | Idiomatic Python interface |
| **Rust** | [`aic-sdk-rs`](https://github.com/ai-coustics/aic-sdk-rs) | Safe Rust bindings with zero-cost abstractions |
| **JavaScript** | [`aic-sdk-node`](https://github.com/ai-coustics/aic-sdk-node) | Native bindings for Node.js applications |
| **Web (WASM)** | [`aic-sdk-wasm`](https://github.com/ai-coustics/aic-sdk-wasm) | WebAssembly build for browser applications |

All SDKs provide the same core functionality with language-specific optimizations and idioms.

### Demo Plugin
Experience our speech enhancement models firsthand with our [Demo Plugin](https://github.com/ai-coustics/aic-sdk-plugin) - a complete audio plugin that showcases all available models while serving as a comprehensive C++ integration example.

### Get Help
Need assistance? We're here to support you:
- **Issues**: [GitHub Issues](https://github.com/ai-coustics/aic-sdk-cpp/issues)
- **Technical Support**: [info@ai-coustics.com](mailto:info@ai-coustics.com)

## License
This C++ wrapper is distributed under the [Apache 2.0 license](LICENSE), while the core C SDK is distributed under the proprietary [AIC-SDK license](LICENSE.AIC-SDK).

---

Made with ❤️ by the ai-coustics team
