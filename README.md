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

### ⚙️ CMake Integration

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

### 📝 Example Usage

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

## 📚 Documentation

- **[Basic Example](example/main.cpp)** - Sample code and integration patterns
- **[CMake Integration Guide](example/CMakeLists.txt)** - Build configuration and SDK integration

## 🆘 Support & Resources

Need help? We're here to assist:

- **Issues**: [GitHub Issues](https://github.com/ai-coustics/aic-sdk-cpp/issues)
- **Technical Support**: [info@ai-coustics.com](mailto:info@ai-coustics.com)

---

Made with ❤️ by the ai-coustics team
