# aic-sdk-cpp - C++ Bindings for ai-coustics SDK

C++ wrapper for the ai-coustics SDK.

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
    GIT_TAG        0.22.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(aic_sdk)

target_link_libraries(my_app PRIVATE aic-sdk)
```

The wrapper downloads the matching ai-coustics C SDK at configure time. Set `AIC_SDK_ROOT` to a
preinstalled C SDK (with `include/` and `lib/`) to skip the download, and `AIC_SDK_USE_STATIC=OFF`
to link the dynamic library instead of the static one.

## Quick Start

All audio APIs are mono. Every buffer you hand to the SDK holds a single channel, so downmix
multichannel audio before passing it in, or create one instance per channel.

Every model runs at every supported sample rate (8000 - 192000 Hz) and at any block size, so you can
initialize for the format your host delivers. `Model::get_optimal_sample_rate` and
`Model::get_optimal_block_size` report the model's native configuration, which avoids internal
resampling and extra buffering and therefore gives the lowest delay.

```cpp
#include "aic.hpp"

#include <cstdlib>
#include <vector>

int main() {
    // Get your license key from the environment variable
    const char* license_key = std::getenv("AIC_SDK_LICENSE");

    // Load a model (download models at https://artifacts.ai-coustics.io/)
    auto model = aic::Model::create_from_file("path/to/model.aicmodel").take();

    // Create processor with license key
    auto processor = aic::Processor::create(model, license_key).take();

    // Get optimal configuration
    auto sample_rate = model.get_optimal_sample_rate();
    auto block_size = model.get_optimal_block_size(sample_rate);

    // Initialize processor with optimal settings
    processor.initialize(sample_rate, block_size, false);

    // Process audio (Mono only)
    std::vector<float> audio(block_size, 0.0f);
    processor.process(audio.data(), audio.size());

    // Cleanup happens in the destructors
    return 0;
}
```

## Usage

### Error Handling

Operations that create an object return a `Result<T>`, holding the value and an `ErrorCode`.
Everything else returns an `ErrorCode` directly. Simple getters, such as
`ProcessorContext::get_parameter`, cannot fail for a valid handle and return the value itself.
The snippets below leave the checks out to stay readable, production code should not.

Most codes report a programming mistake you fix once: `ErrorCode::NullPointer`,
`ErrorCode::NotInitialized`, or `ErrorCode::AudioConfigMismatch` when a block is longer than the
configured `block_size`. The codes worth real handling are the license and authorization ones.

Licensing and model type are checked when you create an object:

```cpp
auto result = aic::Processor::create(model, license_key);

switch (result.error) {
    case aic::ErrorCode::Success:
        break;
    case aic::ErrorCode::LicenseFormatInvalid:
        std::cerr << "License key is malformed\n";
        break;
    case aic::ErrorCode::LicenseExpired:
        std::cerr << "License key has expired\n";
        break;
    case aic::ErrorCode::LicenseVersionUnsupported:
        std::cerr << "License key is not compatible with this SDK version\n";
        break;
    case aic::ErrorCode::ModelTypeUnsupported:
        std::cerr << "Model is not an enhancement or bypass model\n";
        break;
    default:
        std::cerr << "Error: " << static_cast<int>(result.error) << "\n";
        break;
}

auto processor = result.take();
```

`Result<T>::ok()` is shorthand for `error == ErrorCode::Success`, and `take()` moves the value out,
which is how you get these move-only wrappers out of the result:

```cpp
auto result = aic::Model::create_from_file("path/to/model.aicmodel");
if (!result.ok()) {
    // Handle error using result.error
    return 1;
}
auto model = result.take();
```

`Processor::process`, `Vad::process`, and `Analyzer::analyze_buffered` return
`ErrorCode::ProcessingNotAllowed` when the SDK key was not authorized or usage reporting failed,
usually a missing internet connection. This can appear mid-stream and not only at startup, so handle
it where you process audio, not just during setup.

#### Common Error Codes

| Error Code | Description |
|------------|-------------|
| `LicenseFormatInvalid` | License key format is invalid or corrupted |
| `LicenseExpired` | License key has expired |
| `LicenseVersionUnsupported` | License version not compatible with SDK |
| `ModelInvalid` | Model file is invalid or corrupted |
| `ModelVersionUnsupported` | Model version not compatible with SDK |
| `FilePathInvalid` | Path to model file is invalid |
| `ModelTypeUnsupported` | Model type not supported by the requested API (e.g. a VAD model passed to a `Processor`) |
| `AudioConfigUnsupported` | Audio configuration not supported by model |
| `AudioConfigMismatch` | Block size differs from the one given at initialization |
| `NotInitialized` | `initialize` has not been called yet |
| `ParameterOutOfRange` | Parameter value outside acceptable range |
| `ProcessingNotAllowed` | SDK key was not authorized or usage reporting failed |
| `TokenUpdateUnsupported` | In-place token update requires both original and new key to be JWTs |

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
    // Handle error
}
auto model = result.take();
```

A single `Model` can be reused to create multiple processors, VADs, or analyzers, according to the
model type. Each of them keeps the underlying model data alive, so the `Model` may be destroyed
first, in any order.

#### From Memory Buffer

```cpp
// Buffer must be 64-byte aligned and remain valid for the model's lifetime
auto result = aic::Model::create_from_buffer(buffer_ptr, buffer_len);
if (!result.ok()) {
    // Handle error
}
auto model = result.take();
```

### Model Information

```cpp
// Get model ID
std::cout << "Model ID: " << model.get_id() << "\n";

// Get optimal sample rate for the model
uint32_t optimal_rate = model.get_optimal_sample_rate();
std::cout << "Optimal sample rate: " << optimal_rate << " Hz\n";

// Get optimal block size for a specific sample rate
size_t optimal_block_size = model.get_optimal_block_size(48000);
std::cout << "Optimal block size at 48kHz: " << optimal_block_size << "\n";
```

### Configuring the Processor

Initialize the processor for the format you will feed it, either the model's optimal configuration
(see [Model Information](#model-information)) for the lowest delay, or whatever your host delivers:

```cpp
// Create processor with license key
auto processor = aic::Processor::create(model, license_key).take();

// Parameters: sample_rate, block_size, variable_block_size
processor.initialize(sample_rate, block_size, false);

// Any supported configuration works, for example 480 samples at 48 kHz
processor.initialize(48000, 480, false);
```

`variable_block_size` describes how your host delivers audio. Leave it `false` when every call
passes exactly `block_size` samples, which is the common case and the lowest-delay one. Pass `true`
when the block length varies between calls: shorter calls are then accepted, at the cost of extra
buffering and therefore more delay. Calls larger than `block_size` are always rejected with
`ErrorCode::AudioConfigMismatch`.

The same flag exists on `Vad::initialize` and `Collector::initialize` and means the same thing
there.

`aic::ProcessorConfig` bundles the three values if you would rather pass them around as one object:

```cpp
aic::ProcessorConfig config(sample_rate, block_size);  // variable_block_size defaults to false
processor.initialize(config.sample_rate, config.block_size, config.variable_block_size);
```

You can create multiple independent processors from the same enhancement model. Each processor
shares the underlying model data internally.

### Processing Audio

```cpp
// Mono audio block
std::vector<float> audio(block_size, 0.0f);

// Process audio in-place
processor.process(audio.data(), audio.size());
```

For multichannel input, downmix to mono first:

```cpp
std::vector<float> mono(block_size);
for (size_t i = 0; i < block_size; ++i) {
    mono[i] = 0.5f * (left[i] + right[i]);
}

processor.process(mono.data(), mono.size());
```

Or create one `Processor` per channel and call `process` once per channel with that channel's
buffer.

### Processor Context

The processor context provides thread-safe access to processor control APIs:

```cpp
// Create processor context for thread-safe control
auto context = processor.create_context().take();

// Get the delay applied to the audio in samples
size_t delay = context.get_audio_delay();
std::cout << "Audio delay: " << delay << " samples\n";

// Reset processor state (clears internal buffers)
context.reset();

// Set enhancement parameters
context.set_parameter(aic::ProcessorParameter::EnhancementLevel, 0.8f);
context.set_parameter(aic::ProcessorParameter::Bypass, 0.0f);

// Get parameter values
float level = context.get_parameter(aic::ProcessorParameter::EnhancementLevel);
std::cout << "Enhancement level: " << level << "\n";
```

The context may outlive the processor, and destroying it does not destroy the processor.

### Voice Activity Detection (VAD)

Voice activity detection runs on its own object, `aic::Vad`, and needs a dedicated VAD model.
Enhancement models are rejected by `Vad::create` with `ErrorCode::ModelTypeUnsupported`.

#### Creating and Initializing a VAD

```cpp
// Load a dedicated VAD model
auto vad_model = aic::Model::create_from_file("path/to/vad_model.aicmodel").take();

// Create the VAD with your license key
auto vad = aic::Vad::create(vad_model, license_key).take();

// Get optimal configuration for this model
auto sample_rate = vad_model.get_optimal_sample_rate();
auto block_size = vad_model.get_optimal_block_size(sample_rate);

// Initialize before processing any audio
// Parameters: sample_rate, block_size, variable_block_size
vad.initialize(sample_rate, block_size, false);
```

You can create multiple independent VAD instances from the same VAD model. Each VAD shares the
underlying model data internally.

#### VAD Context

The VAD context provides thread-safe access to the VAD's control and query APIs. Create it once
and keep it for as long as you need to read predictions or change parameters:

```cpp
auto vad_context = vad.create_context().take();

// Configure VAD parameters (all can be changed while audio is running)
vad_context.set_parameter(aic::VadParameter::Sensitivity, 0.8f);
vad_context.set_parameter(aic::VadParameter::SpeechHoldDuration, 0.05f);
vad_context.set_parameter(aic::VadParameter::MinimumSpeechDuration, 0.0f);

// Get parameter values
float sensitivity = vad_context.get_parameter(aic::VadParameter::Sensitivity);
std::cout << "VAD sensitivity: " << sensitivity << "\n";

// How far behind its input the prediction is, in samples.
// This delay is not applied to the audio, the VAD never modifies the buffer.
size_t prediction_delay = vad_context.get_prediction_delay();
std::cout << "VAD prediction delay: " << prediction_delay << " samples\n";
```

`Sensitivity` is always a probability threshold between 0.0 and 1.0: the VAD model outputs a speech
probability per block, and a value above the threshold triggers a speech detected decision.

#### Running the VAD

Drive the VAD one mono block at a time, then read the prediction. Unlike `Processor::process`, the
input is read-only and is not modified:

```cpp
std::vector<float> audio(block_size, 0.0f);

// Per audio block: process, then query
vad.process(audio.data(), audio.size());

if (vad_context.is_speech_detected()) {
    std::cout << "Speech detected!\n";
}

// The model's direct output, without the SDK's post-processing
// (speech hold duration, sensitivity thresholding, ...)
float raw_probability = vad_context.get_raw_vad_probability();
```

Reset clears the internal buffers and the published prediction. Call it when the audio stream is
interrupted or when seeking, to prevent mispredictions from the previous content. The VAD stays
initialized:

```cpp
vad_context.reset();
```

The `Vad`, its context and the model can be destroyed in any order. The context may outlive the
VAD, it just stops receiving new data.

### Combining Enhancement and VAD

Enhancement and VAD are independent objects, each with its own model. Run them side by side and
**feed the VAD the original input audio, not the processor's output.**

Create a `Vad` from a VAD model and a `Processor` from an enhancement model as shown above, each
with its own context. Initialize both for the same sample rate and block size, the format your host
delivers. The two models may report different optimal configurations, which does not prevent running
them on a common one.

In your audio callback, pass the same block to both. `Vad::process` does not modify its input, so
calling it first is all it takes to keep the VAD on the unprocessed signal:

```cpp
vad.process(audio.data(), audio.size());        // reads the original input
processor.process(audio.data(), audio.size());  // enhances it in-place

bool is_speech_detected = vad_context.is_speech_detected();
```

Because both objects read the same input, their delays are independent:

```cpp
size_t prediction_delay = vad_context.get_prediction_delay();
size_t audio_delay = proc_context.get_audio_delay();

// The enhanced audio lags the input by audio_delay.
// The VAD prediction lags the same input by prediction_delay.
```

Avoid chaining the two, meaning feeding the processor's output into the VAD. Enhancement is
designed to change the signal, so the VAD would be detecting speech in audio that no longer
matches what its model expects, and the prediction would then lag the original input by
`audio_delay + prediction_delay`.

### Working with the Analyzer

Analysis needs a dedicated analysis model (such as Tyto). Other model types are rejected by
`AnalyzerPair::create` with `ErrorCode::ModelTypeUnsupported`.

Instantiate an analyzer pair:

```cpp
// Create an analyzer pair with your license key
auto pair = aic::AnalyzerPair::create(analysis_model, license_key).take();
auto collector = std::move(pair.collector);  // belongs in the audio thread
auto analyzer = std::move(pair.analyzer);    // run on a separate thread

// Initialize the collector, similar to the processor initialization
collector.initialize(sample_rate, block_size, false);
```

You can create multiple independent analyzer pairs from the same analysis model. Each analyzer
shares the underlying model data internally.

Buffer the audio with `Collector::buffer`, from your audio callback. Unlike `Processor::process`,
the input is read-only and is not modified:

```cpp
// Mono audio block
std::vector<float> audio(block_size, 0.0f);

// Buffer audio for later analysis
collector.buffer(audio.data(), audio.size());
```

The analysis itself does not happen on its own. Nothing runs until you call
`Analyzer::analyze_buffered`, and you have to call it from a thread of your own, never from the
audio thread: analysis models are far too expensive for a real-time callback. That is the whole
reason the collector and the analyzer are separate objects. `Collector::buffer` may keep running on
the paired collector while an analysis is in progress.

The analysis model consumes a fixed length of audio, so if the collector has buffered less than
that, the tail of the input is analyzed as silence:

```cpp
auto result = analyzer.analyze_buffered();
if (result.ok()) {
    const aic::AnalysisResult& scores = result.value;
    std::cout << "Risk score: " << scores.risk_score << "\n";
    std::cout << "Noise: " << scores.noise << "\n";
}
```

Reset clears the buffered audio and the internal state of both the analyzer and its collector. Call
it when the audio stream is interrupted or when seeking. The collector stays initialized:

```cpp
analyzer.reset();
```

The `Collector` and `Analyzer` are independent and can be destroyed in any order.

### JWT Token Refresh

When your license key is a JWT, `update_bearer_token` swaps in a renewed token while audio
processing continues uninterrupted. It is available on `ProcessorContext`, `VadContext` and
`Analyzer`:

```cpp
auto err = context.update_bearer_token(new_jwt_string);
if (err != aic::ErrorCode::Success) {
    std::cerr << "Token update failed: " << static_cast<int>(err) << "\n";
}
```

Both the original key and the new token must be JWTs. If either is not, the call returns
`ErrorCode::TokenUpdateUnsupported` and the existing token stays in use. On any error the call is a
no-op, so processing is never interrupted by a failed refresh.

### Session Termination

Telemetry sessions are closed automatically when a `Processor`, `Vad` or `Analyzer` is destroyed.
Call `terminate_session` to close one on demand during a lifecycle event, for example when the
object's destruction may be delayed:

```cpp
processor.terminate_session();
vad.terminate_session();
analyzer.terminate_session();
```

After termination the object is no longer allowed to process audio.

### OpenTelemetry

Pass an `OtelConfig` to `Processor::create` or `Vad::create` to enable telemetry for a specific
session:

```cpp
aic::OtelConfig otel;
otel.enable = true;
otel.session_id = "my-session-id";  // nullptr = auto-generate
otel.export_interval_ms = 5000;     // 0 = default (60 000 ms)

auto processor = aic::Processor::create(model, license_key, &otel).take();
```

Alternatively, pass `nullptr` (the default) and set `AIC_SDK_OTEL_ENABLE=1` in the environment to
enable telemetry globally.

## Examples

See the [`example/enhancement.cpp`](example/enhancement.cpp), [`example/vad.cpp`](example/vad.cpp)
and [`example/analyzer.cpp`](example/analyzer.cpp) files for complete working examples. Enhancement,
VAD and analysis are separate objects with separate models, so each has its own self-contained
example.

Set your license key in an environment variable:

```bash
export AIC_SDK_LICENSE="your_license_key_here"
```

Build all three examples:

```sh
cmake -B build ./example
cmake --build build -j
```

Run the enhancement example with an enhancement or bypass model:

```sh
./build/aic-sdk-enhancement path/to/model.aicmodel
```

Run the VAD example with a dedicated VAD model:

```sh
./build/aic-sdk-vad path/to/vad-model.aicmodel
```

Run the analyzer example with a dedicated analysis model:

```sh
./build/aic-sdk-analyzer path/to/analysis-model.aicmodel
```

**Note:** Each example rejects the other examples' model types with
`ErrorCode::ModelTypeUnsupported`. Voice activity detection requires a dedicated VAD model and
analysis a dedicated analysis model; neither can run on an enhancement model.

## Compatibility

The wrapper is fully C++11 compatible. On Linux, you will need at least GLIBC 2.27 (Ubuntu 18.04).

## Documentation

- **Full Documentation**: [docs.ai-coustics.com](https://docs.ai-coustics.com)
- **C++ API Reference**: See the [header file](include/aic.hpp) for detailed API documentation
- **Available Models**: [artifacts.ai-coustics.io](https://artifacts.ai-coustics.io)

## License

This C++ wrapper is distributed under the Apache 2.0 license. The core SDK library is distributed under the proprietary AIC-SDK license.
