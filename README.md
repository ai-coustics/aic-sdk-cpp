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

### Download the SDK

Get started by downloading the SDK binaries from the [releases page](https://github.com/ai-coustics/aic-sdk-c/releases). We provide:

- **Static libraries** (`.a`, `.lib`) for linking directly into your application
- **Dynamic libraries** (`.so`, `.dll`, `.dylib`) for runtime loading
- **Multiple platforms**: Linux, Windows, macOS
- **Multiple architectures**: x86_64, ARM64

### Language Bindings

While this repository contains the C++ wrapper, we offer convenient wrappers for popular programming languages:

| Language | Repository | Description |
|----------|------------|-------------|
| **C** | [`aic-sdk-c`](https://github.com/ai-coustics/aic-sdk-c) | Core C interface and foundation library |
| **C++** | [`aic-sdk-cpp`](https://github.com/ai-coustics/aic-sdk-cpp) | Modern C++ wrapper with RAII and type safety |
| **Node.js** | [`aic-sdk-node`](https://github.com/ai-coustics/aic-sdk-node) | JavaScript/TypeScript bindings for Node.js |
| **Python** | [`aic-sdk-py`](https://github.com/ai-coustics/aic-sdk-py) | Pythonic interface |
| **Rust** | [`aic-sdk-rs`](https://github.com/ai-coustics/aic-sdk-rs) | Safe Rust Bindings |
| **WebAssembly** | [`aic-sdk-wasm`](https://github.com/ai-coustics/aic-sdk-wasm) | Browser-compatible WebAssembly build |

### Demo Plugin

Experience our speech enhancement models firsthand with our Demo Plugin - a complete audio plugin that showcases all available models while serving as a comprehensive C++ integration example.

| Platform | Repository | Description |
|----------|------------|-------------|
| **Demo Plugin** | [`aic-sdk-plugin`](https://github.com/ai-coustics/aic-sdk-plugin) | Audio plugin with real-time model comparison and C++ integration reference |

The Demo Plugin provides:
- **Real-time audio processing** with instant A/B comparison
- **All model variants** available for testing and evaluation
- **Production-ready C++ code** demonstrating best practices
- **Cross-platform compatibility** (VST3, AU, Standalone)

## 📚 Documentation

- **[SDK Reference](sdk-reference.md)** - Complete API documentation and function reference
- **[Basic Example](example/main.cpp)** - Sample code and integration patterns

## 💡 Example Usage

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

To build and run the provided example code:

### CMake Configuration

Download pre-built library during configure step:

```sh
cmake -B build -DAIC_SDK_ALLOW_DOWNLOAD=ON
```

Use local library:

```sh
cmake -B build -DAIC_SDK_ROOT=/path/to/aic-sdk/
```

### Run Example

```sh
cmake -B build ./example
cmake --build build -j
./build/my_app
```

To run the example, make sure you have set your license key as an environment variable:

```bash
export AIC_SDK_LICENSE="your_license_key_here"
```

## 🆘 Support & Resources

Need help? We're here to assist:

- **Documentation**: [Complete SDK Reference](sdk-reference.md)
- **Examples**: [Sample Code](example/main.cpp)
- **Issues**: [GitHub Issues](https://github.com/ai-coustics/aic-sdk-cpp/issues)
- **Technical Support**: [info@ai-coustics.com](mailto:info@ai-coustics.com)

---

Made with ❤️ by the ai-coustics team
