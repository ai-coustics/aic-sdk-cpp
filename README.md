# aic-sdk-cpp - C++ Bindings for ai-coustics SDK

C++ wrapper for the ai-coustics Speech Enhancement SDK.

For comprehensive documentation, visit [docs.ai-coustics.com](https://docs.ai-coustics.com).

> [!NOTE]
> This SDK requires a license key. Generate your key at [developers.ai-coustics.com](https://developers.ai-coustics.com).

## Installation

### CMake Integration

```cmake
include(FetchContent)

set(AIC_SDK_ALLOW_DOWNLOAD ON CACHE BOOL "Allow C SDK download at configure time")

FetchContent_Declare(
    aic_sdk
    GIT_REPOSITORY https://github.com/ai-coustics/aic-sdk-cpp.git
    GIT_TAG        0.21.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(aic_sdk)

target_link_libraries(my_app PRIVATE aic-sdk)
```

## Quick Start

```cpp
#include "aic.hpp"

#include <vector>
#include <cstdlib>

int main() {
    // Get your license key from the environment variable
    const char* license_key = std::getenv("AIC_SDK_LICENSE");

    // Load a model from file (download models at https://artifacts.ai-coustics.io/)
    auto model = aic::Model::create_from_file("./models/model.aicmodel").take();

    // Create configuration with model's optimal settings
    auto sample_rate = model.get_optimal_sample_rate();
    auto num_frames = model.get_optimal_num_frames(sample_rate);
    aic::ProcessorConfig config(sample_rate, num_frames, 2);  // 2 channels (stereo)

    // Create and initialize processor
    auto processor = aic::Processor::create(model, license_key).take();
    processor.initialize(config.sample_rate, config.num_channels, config.num_frames,
                         config.allow_variable_frames);

    // Process audio (planar layout: separate buffer per channel)
    std::vector<float> left(config.num_frames, 0.0f);
    std::vector<float> right(config.num_frames, 0.0f);
    std::vector<float*> audio = {left.data(), right.data()};
    processor.process_planar(audio.data(), config.num_channels, config.num_frames);

    return 0;
}
```

## Usage

### SDK Information

```cpp
// Get SDK version
std::cout << "SDK version: " << aic::get_sdk_version() << "\n";

// Get compatible model version
std::cout << "Compatible model version: " << aic::get_compatible_model_version() << "\n";
```

### Loading Models

Download models and find available IDs at [artifacts.ai-coustics.io](https://artifacts.ai-coustics.io/).

#### From File
```cpp
auto result = aic::Model::create_from_file("path/to/model.aicmodel");
if (!result.ok()) {
    // Handle error using result.error
}
auto model = result.take();
```

#### From Memory Buffer
```cpp
// Buffer must be 64-byte aligned and remain valid for the model's lifetime
auto result = aic::Model::create_from_buffer(buffer_ptr, buffer_size);
if (!result.ok()) {
    // Handle error using result.error
}
auto model = result.take();
```

### Model Information

```cpp
// Get model ID
std::string model_id = model.get_id();

// Get optimal sample rate for the model
uint32_t optimal_rate = model.get_optimal_sample_rate();

// Get optimal frame count for a specific sample rate
size_t optimal_frames = model.get_optimal_num_frames(48000);
```

### Configuring the Processor

```cpp
// Query optimal settings from the model
auto sample_rate = model.get_optimal_sample_rate();
auto num_frames = model.get_optimal_num_frames(sample_rate);

// Create configuration with optimal settings
aic::ProcessorConfig config(sample_rate, num_frames);  // mono, fixed frames
// Or customize: stereo with variable frames
aic::ProcessorConfig config(sample_rate, num_frames, 2, true);

// Or use custom values entirely
aic::ProcessorConfig config(48000, 480, 2, false);

// Create processor with license key
auto processor = aic::Processor::create(model, license_key).take();

// Initialize with configuration
processor.initialize(config.sample_rate, config.num_channels, config.num_frames,
                     config.allow_variable_frames);
```

### Processing Audio

The SDK supports three audio buffer layouts for in-place processing:

#### Planar (Separate Buffer per Channel)
```cpp
// Each channel in its own buffer
std::vector<float> left(num_frames, 0.0f);
std::vector<float> right(num_frames, 0.0f);
std::vector<float*> audio = {left.data(), right.data()};

processor.process_planar(audio.data(), num_channels, num_frames);
```

#### Interleaved (Alternating Samples)
```cpp
// [L0, R0, L1, R1, L2, R2, ...]
std::vector<float> audio(num_channels * num_frames);

processor.process_interleaved(audio.data(), num_channels, num_frames);
```

#### Sequential (Channel Data Concatenated)
```cpp
// [L0, L1, L2, ..., R0, R1, R2, ...]
std::vector<float> audio(num_channels * num_frames);

processor.process_sequential(audio.data(), num_channels, num_frames);
```

### Processor Context

```cpp
// Get processor context
auto ctx = processor.create_context().take();

// Get output delay in samples
size_t delay = ctx.get_output_delay();

// Reset processor state (clears internal buffers)
ctx.reset();

// Set enhancement parameters
ctx.set_parameter(aic::ProcessorParameter::EnhancementLevel, 0.8f);
ctx.set_parameter(aic::ProcessorParameter::Bypass, 1.0f);

// Get parameter values
float level = ctx.get_parameter(aic::ProcessorParameter::EnhancementLevel);
std::cout << "Enhancement level: " << level << "\n";
```

### JWT Token Refresh

When using a JWT license key, use `update_bearer_token` to swap in a renewed token while audio processing continues uninterrupted:

```cpp
auto ctx = processor.create_context().take();

// Later, when the JWT needs renewal:
auto err = ctx.update_bearer_token(new_jwt_string);
if (err != aic::ErrorCode::Success) {
    std::cerr << "Token update failed: " << static_cast<int>(err) << "\n";
}
```

Both the original key and the new token must be JWTs. If either is not, the call returns `ErrorCode::TokenUpdateUnsupported`.

### OpenTelemetry

Pass an `OtelConfig` to `Processor::create` to enable telemetry for a specific session:

```cpp
aic::OtelConfig otel;
otel.enable = true;
otel.session_id = "my-session-id";  // nullptr = auto-generate
otel.export_interval_ms = 5000;     // 0 = default (60 000 ms)

auto processor = aic::Processor::create(model, license_key, &otel).take();
```

Alternatively, pass `nullptr` (the default) and set `AIC_SDK_OTEL_ENABLE=1` in the environment to enable telemetry globally.

### Voice Activity Detection (VAD)

```cpp
// Get VAD context from processor
auto vad = processor.create_vad_context().take();

// Configure VAD parameters
vad.set_parameter(aic::VadParameter::Sensitivity, 6.0f);
vad.set_parameter(aic::VadParameter::SpeechHoldDuration, 0.05f);
vad.set_parameter(aic::VadParameter::MinimumSpeechDuration, 0.0f);

// Get parameter values
float sensitivity = vad.get_parameter(aic::VadParameter::Sensitivity);
std::cout << "VAD sensitivity: " << sensitivity << "\n";

// Check for speech (after processing audio through the processor)
if (vad.is_speech_detected()) {
    std::cout << "Speech detected!\n";
}
```

### Audio Analysis

Analysis models (such as Tyto) score audio for problems that can degrade downstream models. Analysis is split into two pieces: a `Collector` that buffers audio in the real-time thread, and an `Analyzer` that runs the (expensive) analysis model on a separate thread.

```cpp
// Create the pair from an analysis model
auto pair = aic::AnalyzerPair::create(model, license_key).take();
auto collector = std::move(pair.collector);  // belongs in the audio thread
auto analyzer = std::move(pair.analyzer);    // run on a separate thread

// Configure the collector like a processor
collector.initialize(sample_rate, num_channels, num_frames, false);

// In the audio thread: buffer audio (input is read-only, not modified)
collector.buffer_interleaved(audio.data(), num_channels, num_frames);

// On another thread: analyze the latest buffered audio
auto result = analyzer.analyze_buffered();
if (result.ok()) {
    const aic::AnalysisResult& scores = result.value;
    std::cout << "Risk score: " << scores.risk_score << "\n";
    std::cout << "Noise: " << scores.noise << "\n";
}
```

`AnalyzerPair::create` returns `ErrorCode::ModelTypeUnsupported` if the model is not an analysis model. The `Collector` and `Analyzer` are independent and can be destroyed in any order.

### Error Handling

The SDK uses a `Result<T>` type for operations that can fail. All error conditions are represented by the `aic::ErrorCode` enum.

#### Checking Results

```cpp
auto result = aic::Model::create_from_file("path/to/model.aicmodel");
if (!result.ok()) {
    switch (result.error) {
        case aic::ErrorCode::ModelFilePathInvalid:
            std::cerr << "Invalid model path\n";
            break;
        case aic::ErrorCode::ModelInvalid:
            std::cerr << "Invalid model file\n";
            break;
        case aic::ErrorCode::ModelVersionUnsupported:
            std::cerr << "Model version not supported\n";
            break;
        default:
            std::cerr << "Error code: " << static_cast<int>(result.error) << "\n";
    }
    return 1;
}
auto model = result.take();
```

#### Common Error Codes

| Error Code | Description |
|------------|-------------|
| `LicenseFormatInvalid` | License key format is invalid or corrupted |
| `LicenseExpired` | License key has expired |
| `LicenseVersionUnsupported` | License version not compatible with SDK |
| `ModelInvalid` | Model file is invalid or corrupted |
| `ModelVersionUnsupported` | Model version not compatible with SDK |
| `ModelFilePathInvalid` | Path to model file is invalid |
| `AudioConfigUnsupported` | Audio configuration not supported by model |
| `ParameterOutOfRange` | Parameter value outside acceptable range |
| `TokenUpdateUnsupported` | In-place token update requires both original and new key to be JWTs |
| `ModelTypeUnsupported` | Model type not supported by the processor or analyzer (e.g. wrong model kind) |

## Building and Running the Example

Set your license key in an environment variable:

```bash
export AIC_SDK_LICENSE="your_license_key_here"
```

Build and run the example:

```sh
cmake -B build ./example
cmake --build build -j
./build/my_app /path/to/model.aicmodel
```

### Compatibility

The wrapper is fully C++11 compatible. On Linux, you will need at least GLIBC 2.27 (Ubuntu 18.04).

## Examples

See the [`example/main.cpp`](example/main.cpp) file for a complete working example demonstrating all SDK features including model loading, processor configuration, parameter adjustment, VAD usage, and all three audio processing layouts.

## Documentation

- **Full Documentation**: [docs.ai-coustics.com](https://docs.ai-coustics.com)
- **C++ API Reference**: See the [header file](include/aic.hpp) for detailed type information
- **Available Models**: [artifacts.ai-coustics.io](https://artifacts.ai-coustics.io)

## License

This C++ wrapper is distributed under the Apache 2.0 license. The core C SDK is distributed under the proprietary AIC-SDK license.
