# Changelog

All notable changes to this project are documented here. This project adheres to
[Semantic Versioning](https://semver.org/).

## 0.23.0 - 2026-08-11

### Breaking Changes

#### New model file version

This release requires model file version 7. Re-download your models so the SDK does not reject them
with `ErrorCode::ModelVersionUnsupported`. See the
[compatibility matrix](https://docs.ai-coustics.com/reference/sdk/compatibility-matrix).

Model files are available at [artifacts.ai-coustics.io](https://artifacts.ai-coustics.io/), and the
version this SDK build expects can be queried at runtime:

```cpp
std::cout << "Compatible model version: " << aic::get_compatible_model_version() << "\n";
```

### New Features

#### Tyto 1.0 has been replaced by Tyto 1.1

The analysis model is now Tyto 1.1, `tyto-1.1-l-16khz`. Tyto 1.0 (`tyto-l-16khz`) is not loadable by
this SDK version any more: it has no model file version 7 release, so
`aic::Model::create_from_file` rejects it with `ErrorCode::ModelVersionUnsupported`.

#### Analysis result fields changed

`aic::AnalysisResult` gained a field and lost one:

- Added: `codec_degradation`, a measure of artifacts introduced by lossy speech codecs, e.g. from a
  low bitrate or a narrowband codec.
- Removed: `media_speech`.

Code that reads the scores by name only needs updating where it referenced `media_speech`:

```cpp
auto result = analyzer.analyze_buffered();
if (result.ok()) {
    const aic::AnalysisResult& scores = result.value;

    std::cout << "risk_score:         " << scores.risk_score << "\n";
    std::cout << "speaker_reverb:     " << scores.speaker_reverb << "\n";
    std::cout << "speaker_loudness:   " << scores.speaker_loudness << "\n";
    std::cout << "interfering_speech: " << scores.interfering_speech << "\n";
    std::cout << "noise:              " << scores.noise << "\n";
    std::cout << "codec_degradation:  " << scores.codec_degradation << "\n";
    std::cout << "packet_loss:        " << scores.packet_loss << "\n";
}
```

Brace initialization of `aic::AnalysisResult` is positional, so any such literal in your code has to
be revisited: the field after `interfering_speech` is now `noise`, followed by
`codec_degradation`.

### Bug Fixes

- `Analyzer::analyze_buffered` no longer crashes when OpenTelemetry reporting is enabled.

## 0.22.0 - 2026-08-07

This release contains two breaking changes that affect every integration:

- **All audio APIs are mono only.** The multi-channel process/buffer methods are gone.
- **The VAD is its own object.** VAD runs on dedicated VAD models through the new `aic::Vad` class,
  and can no longer be derived from a `Processor`.

Both migrations are covered step by step below.

### Breaking Changes

#### Multi-channel support removed

`Processor` and the analyzer's `Collector` now operate on mono audio only.

All models process mono inputs. Previously the processor mixed all input channels down to mono
internally, which could lead to surprising results. To prevent misunderstandings, all APIs now take
exclusively mono inputs.

To process multi-channel audio, downmix to mono before calling `Processor::process`, or create a
separate `Processor` instance per channel.

##### What changed

| Before (0.21.0) | Now |
| --- | --- |
| `processor.initialize(sample_rate, num_channels, num_frames, allow_variable_frames)` | `processor.initialize(sample_rate, block_size, variable_block_size)` |
| `processor.process_planar` / `process_interleaved` / `process_sequential` | `processor.process(audio, audio_len)` |
| `collector.initialize(sample_rate, num_channels, num_frames, allow_variable_frames)` | `collector.initialize(sample_rate, block_size, variable_block_size)` |
| `collector.buffer_planar` / `buffer_interleaved` / `buffer_sequential` | `collector.buffer(audio, audio_len)` |
| `model.get_optimal_num_frames(sample_rate)` | `model.get_optimal_block_size(sample_rate)` |
| `aic::ProcessorConfig(sample_rate, num_frames, num_channels, allow_variable_frames)` | `aic::ProcessorConfig(sample_rate, block_size, variable_block_size)` |

Naming also became consistent: the configured block length is `block_size`, the per-call buffer
length is `audio_len`, and the `allow_variable_frames` flag is now `variable_block_size`.

##### Before

```cpp
auto sample_rate = model.get_optimal_sample_rate();
auto num_frames  = model.get_optimal_num_frames(sample_rate);

// Stereo in, stereo out. The SDK mixed both channels down to mono internally.
processor.initialize(sample_rate, 2, num_frames, false);

std::vector<float*> planar = {left.data(), right.data()};
processor.process_planar(planar.data(), 2, num_frames);
```

##### After

```cpp
auto sample_rate = model.get_optimal_sample_rate();
auto block_size  = model.get_optimal_block_size(sample_rate);

processor.initialize(sample_rate, block_size, false);

// Downmix to mono yourself, then process a single buffer in-place.
std::vector<float> mono(block_size);
for (size_t i = 0; i < block_size; ++i) {
    mono[i] = 0.5f * (left[i] + right[i]);
}

processor.process(mono.data(), mono.size());
```

If you need per-channel output instead of a downmix, create one `Processor` per channel and call
`Processor::process` once per channel with that channel's buffer.

The same applies to the analyzer: downmix multi-channel audio before calling `Collector::buffer`,
or create a separate `AnalyzerPair` per channel.

The OpenTelemetry `audio.channels` metric has been kept for backwards compatibility, but it now
always reports exactly one channel.

#### VAD moved into its own object, energy-based VAD removed

Voice activity detection is no longer a side effect of enhancement. It is now a first-class type,
`aic::Vad`, that runs a dedicated VAD model.

Energy-based VADs, which inferred speech activity from the output level of an enhancement model,
have been removed. They were an approximation and their accuracy depended on the enhancement model
in use. A dedicated VAD model is trained for the task and is considerably more accurate.

##### What changed

| Before (0.21.0) | Now |
| --- | --- |
| VAD came from a processor: `processor.create_vad_context()` | VAD is standalone: `aic::Vad::create` → `vad.initialize` → `vad.process`, then `vad.create_context()` |
| Any enhancement model provided a VAD (energy-based), VAD models were also loaded into a `Processor` | Only dedicated VAD models are accepted by `aic::Vad::create`; `aic::Processor::create` accepts only enhancement and bypass models |
| VAD advanced whenever the processor processed audio | VAD advances on `Vad::process`, independently of any processor |
| `VadParameter::Sensitivity` ranged 0.0 - 1.0 on VAD models and 1.0 - 15.0 on energy-based VADs | `VadParameter::Sensitivity` is always a probability threshold, 0.0 - 1.0 |

##### Before

```cpp
// One model, one processor: enhancement and VAD were coupled.
auto processor = aic::Processor::create(enhancement_model, license_key).take();
processor.initialize(sample_rate, 1, num_frames, false);

auto vad = processor.create_vad_context().take();
vad.set_parameter(aic::VadParameter::Sensitivity, 5.0f); // energy threshold

// The VAD updated as a side effect of enhancement.
processor.process_interleaved(audio.data(), 1, num_frames);

bool is_speech = vad.is_speech_detected();
```

##### After

```cpp
// Load a dedicated VAD model and create an aic::Vad from it.
auto vad_model = aic::Model::create_from_file("path/to/vad-model.aicmodel").take();

// Returns ErrorCode::ModelTypeUnsupported if the model is not a VAD model.
auto vad = aic::Vad::create(vad_model, license_key).take();

auto sample_rate = vad_model.get_optimal_sample_rate();
auto block_size  = vad_model.get_optimal_block_size(sample_rate);
vad.initialize(sample_rate, block_size, false);

auto vad_ctx = vad.create_context().take();
vad_ctx.set_parameter(aic::VadParameter::Sensitivity, 0.8f); // probability

// The VAD is driven explicitly and does not modify the audio.
vad.process(audio.data(), audio.size());

bool is_speech = vad_ctx.is_speech_detected();
```

##### Run the VAD on the original audio

If you use enhancement and VAD together, **feed the VAD the original input audio, not the
processor's enhanced output.** Run the two objects side by side on the same mono block rather than
chaining them:

```cpp
// Recommended: both objects see the same original input block.
vad.process(input.data(), input.size());       // reads the block, does not modify it
processor.process(input.data(), input.size()); // enhances the block in-place
```

`Vad::process` takes a `const float*` and leaves the buffer untouched, so calling it on the same
buffer before `Processor::process` is all it takes to keep the VAD on the unprocessed signal.

Enhancement is designed to change the signal, so running the VAD on its output means detecting
speech in audio that no longer matches what the VAD model expects. It also stacks the processor's
delay on top of the VAD's own prediction delay, which makes speech decisions harder to align.

##### Delay queries renamed

There is no single "output delay" any more. The processor delays audio, the VAD does not, so the
two queries are now named after what they actually report:

| Before (0.21.0) | Now | What it means |
| --- | --- | --- |
| `ProcessorContext::get_output_delay` | `ProcessorContext::get_audio_delay` | An **audio** delay. The enhanced samples leave `Processor::process` that many samples behind their input. |
| No VAD-specific query. The VAD was driven by a processor, so its prediction delay was the processor's `get_output_delay`. | `VadContext::get_prediction_delay` | A **prediction** delay. It is *not* applied to the audio, `Vad::process` leaves the buffer untouched. It tells you how far behind its own input the published prediction is, so you can line speech decisions up with the audio timeline. |

With both objects fed from the same input block, the two delays are independent of each other:

```cpp
size_t audio_delay      = ctx.get_audio_delay();
size_t prediction_delay = vad_ctx.get_prediction_delay();

// The enhanced audio lags the input by audio_delay.
// The VAD prediction lags the same input by prediction_delay.
```

`aic::Vad` mirrors the processor's lifecycle and control surface:

- `aic::Vad::create`, `Vad::initialize`, `Vad::process`
- `Vad::create_context` for a thread-safe control handle
- `VadContext::reset`, `VadContext::get_prediction_delay`, `VadContext::update_bearer_token`
- `VadContext::is_speech_detected`, `VadContext::get_raw_vad_probability`,
  `VadContext::set_parameter`, `VadContext::get_parameter`

The OpenTelemetry `experimental.vad.speech_duration` metric is now reported for dedicated VAD
models only. Enhancement and bypass sessions no longer derive VAD state from the model output, so
they always report zero. `processor.vad_created` reports whether the session runs a VAD model.

#### Renamed error codes

Some error codes are no longer processor-specific, since they are now also returned by the VAD:

| Before (0.21.0) | Now |
| --- | --- |
| `ErrorCode::ProcessorNotInitialized` | `ErrorCode::NotInitialized` |
| `ErrorCode::EnhancementNotAllowed` | `ErrorCode::ProcessingNotAllowed` |
| `ErrorCode::ModelFilePathInvalid` | `ErrorCode::FilePathInvalid` |

The numeric values are unchanged, so only source-level references need updating.

### New Features

There are new explicit session termination APIs. Use these to close telemetry sessions during
lifecycle events without waiting for the corresponding object to be destroyed, which is useful in
integrations where destruction may be delayed.

- `Processor::terminate_session`
- `Vad::terminate_session`
- `Analyzer::terminate_session`

### Bug Fixes

- Resetting VAD state now immediately clears the published speech detection and raw VAD
  probability values, so `VadContext::is_speech_detected` and
  `VadContext::get_raw_vad_probability` no longer return stale values from the previous stream
  after `VadContext::reset`.

## 0.21.0 - 2026-06-23

### New Features

This release includes a new `aic::VadContext::get_raw_vad_probability()` API to read the raw output of a VAD model.

### Changes

Reduced the necessary output delay of the `Processor` when using `allow_variable_frames = true`.

## 0.20.0 - 2026-06-16

### New Features

This release adds support for our newest audio intelligence model, *Tyto*, through two new types: `Collector` and `Analyzer`.

- The `Collector` is placed in the audio thread and buffers audio chunks for later analysis. Initialize it with the same configuration as your `Processor` and feed it audio with the `buffer_planar` / `buffer_interleaved` / `buffer_sequential` methods, mirroring the `Processor` process methods. The input audio is read-only and is not modified.
- The `Analyzer` runs separately, since analysis models are too expensive for the audio thread. Call `analyze_buffered` from another thread to obtain an `AnalysisResult` for the latest buffered audio.

Create the pair with `AnalyzerPair::create(model, license_key)`, then move the `collector` and `analyzer` to their respective threads. A JWT token can be refreshed on a running analyzer with `Analyzer::update_bearer_token`.

### Breaking Changes

- Compatible model file version was bumped to 5. Models built for earlier versions are no longer supported.

## 0.19.0 - 2026-06-15

### New Features

- Added support for OpenTelemetry observability. Pass an `aic::OtelConfig` to `Processor::create` to enable telemetry for a specific session, or pass `nullptr` and set the `AIC_SDK_OTEL_ENABLE` environment variable to enable it globally. The metric export interval is configurable via `OtelConfig::export_interval_ms` (0 keeps the default of 60 000 ms).
- Added support for JWT license keys. Pass a JWT string as the license key to `Processor::create`, then use `ProcessorContext::update_bearer_token` to swap in a renewed token while audio processing continues uninterrupted. In-place updates require both the original and the new key to be JWTs; otherwise the call returns the new `ErrorCode::TokenUpdateUnsupported` and the existing token stays in use.
- Added support for dedicated voice activity detection models, which output a per-buffer speech probability. `VadParameter::Sensitivity` is now interpreted based on the active model: 0.0 to 1.0 for VAD models (the probability threshold above which speech is reported), or 1.0 to 15.0 for energy-based VADs (energy threshold = 10 ^ (-sensitivity)). The default sensitivity is now model-specific.

### Breaking Changes

- Compatible model file version was bumped to 4. Models built for earlier versions are no longer supported.

## 0.17.1 - 2026-05-07

### Improvements

- Increased maximum VAD speech hold duration from 100x to 300x the model's window size.

### Bug Fixes

- Removed zero-padding when the host frame size does not match the model frame size, which caused unexpected behavior for some models.

## 0.17.0 - 2026-04-24

### New Features

- Added support for Quail Voice Focus 2.1 models.
- This release adds an **experimental** feature to export real-time audio processing metrics via OpenTelemetry (OTel).
  The new feature is currently disabled by default and available for testing on early access only.

### Breaking Changes

- Quail Voice Focus 2.0 is no longer supported.
- Compatible model file version was bumped to 3.

### Improvements

- Improved performance of telemetry when using multiple processors.

### Fixes

- The scaling factor of the STFT now changes depending on the sample rate.

## 0.15.0 - 2026-02-27

### New features

- Support for V2 model files, which includes support for the new Quail Voice Focus 2.0 model.

### Improvements

- The parameters of Quail models are no longer fixed. The enhancement level of every model can now be adjusted between 0.0 and 1.0.

### Breaking Changes

- V1 model files are no longer supported.
- The error `ErrorCode::ParameterFixed` was removed.
- The parameter `ProcessorParameter::VoiceGain` was removed.
- The parameter `VadParameter::SpeechHoldDuration` previously held detected speech for half of the specified duration. It has now been changed to better represent the intention of the developer.
- The default value for `VadParameter::SpeechHoldDuration` was changed from 50 ms to 30 ms to match the existing behavior.

### Fixes

- `VadContext.setParameter` no longer returns an error when trying to set a valid speech hold duration value before calling `Processor.initialize`.

## 0.14.0 - 2026-01-24

This release comes with a number of new features and several breaking changes. Most notably, the C library does no longer include any models, which significantly reduces the library's binary size. The models are now available separately for download at https://artifacts.ai-coustics.io.

**New license keys required**: License keys previously generated in the [developer portal](https://developers.ai-coustics.io) will no longer work. New license keys must be generated.

**Model naming changes**: Quail-STT models are now called "Quail" - These models are optimized for human-to-machine enhancement (e.g., Speech-to-Text (STT) applications). Quail models are now called "Sparrow" - These models are optimized for human-to-human enhancement (e.g., voice calls, conferencing). This naming change clarifies the distinction between STT-focused models and human-to-human communication model

**Major architectural changes**: The API has been restructured to separate model data from processing instances. What was previously called `aic::AicModel` (which handled both model data and processing) has been split into:
- `aic::Model`: Now represents only the ML model data loaded from files or memory
- `aic::Processor`: New type that performs the actual audio processing using a model
- Multiple processors can share the same model, allowing efficient resource usage across streams
- Models can be destroyed after creating processors; memory is freed when the last processor using them is destroyed
- To change parameters, reset the processor and get output delay, create a `aic::ProcessorContext`. This context can be freely moved between threads

**C++ wrapper alignment**: The C++ API mirrors these changes:
- `aic::Model` holds model data; `aic::Processor` performs processing using a model
- `aic::ProcessorContext` and `aic::VadContext` are created from a processor for thread-safe control APIs
- `aic::ProcessorConfig` struct holds audio configuration (sample rate, channels, frames, variable frame support)

### New features

- Models now load from files via `aic::Model::create_from_file`.
- Models can also be created from in-memory buffers with `aic::Model::create_from_buffer`.
- Added `aic::Model::get_id` to query the id of a model.
- A single model instance can be shared across multiple processors.
- Added `aic::Processor::create` so each stream can be initialized independently from a shared model while sharing weights.
- Added `aic::get_compatible_model_version` to query the required model version for this SDK.
- VAD speech hold duration parameter cap was increased to 100x the model's window length.
- Added context-based APIs for thread-safe control operations:
    - `aic::ProcessorContext` for processor control and queries
    - `aic::VadContext` for VAD control and queries
- Model query APIs moved to model methods:
    - `aic::Model::get_optimal_sample_rate` - gets optimal sample rate for a model
    - `aic::Model::get_optimal_num_frames` - gets optimal frame count for a model at given sample rate
- Added new error codes for model loading (see `aic::ErrorCode`):
    - `ErrorCode::ModelInvalid`
    - `ErrorCode::ModelVersionUnsupported`
    - `ErrorCode::ModelFilePathInvalid`
    - `ErrorCode::FileSystemError`
    - `ErrorCode::ModelDataUnaligned`
- C++ wrapper additions:
    - `aic::Model::create_from_file` and `aic::Model::create_from_buffer`
    - `aic::Processor::create` plus `aic::ProcessorContext` and `aic::VadContext`
    - `aic::Model::get_id`, `aic::Model::get_optimal_sample_rate`, `aic::Model::get_optimal_num_frames`
    - `aic::get_compatible_model_version` and `aic::get_sdk_version`
    - `aic::Result<T>` return type for error handling without exceptions

### Breaking changes

- License keys previously generated in the [developer portal](developers.ai-coustics.io) will no longer work. New license keys have to be generated.
- Existing model-processing combined API has been split into `aic::Model` and `aic::Processor`.
- Removed `aic::ModelType` enum; callers must supply a model file or aligned buffer instead of selecting a built-in model.
- License keys are now provided to `aic::Processor::create` rather than model creation.
- Renamed `aic::Parameter` to `aic::ProcessorParameter`.
- VAD APIs now use `aic::VadContext` created from a processor.
- Processor control APIs now live on `aic::ProcessorContext` (reset, parameter access, output delay).
- Model query APIs now live on `aic::Model` (optimal sample rate/frames).
- `ErrorCode::ModelNotInitialized` was renamed to `ErrorCode::ProcessorNotInitialized`.
- `aic::AicModel` wrapper replaced by `aic::Model` + `aic::Processor`
- Model selection by enum removed; supply a model file or aligned buffer
- Parameter/reset/output delay APIs now live on `aic::ProcessorContext`
- VAD APIs now use `aic::VadContext` created from a processor
- `std::pair`/`std::unique_ptr` creation patterns replaced by `aic::Result<T>` + `take()`

### Fixes

- Improved thread safety.
- Fixed an issue where the allocated size for an FFT operation could be incorrect, leading to a crash.

## 0.12.0 - 2025-12-16

#### Features

- **New VAD parameter `VadParameter::MinimumSpeechDuration`**: Controls for how long speech needs to be present in the audio signal before the VAD considers it speech (0.0 - 1.0 seconds).

#### Breaking Changes

- **Replaced VAD parameter `VadParameter::LookbackBufferSize` with `VadParameter::SpeechHoldDuration`**: The new parameter controls for how long the VAD continues to detect speech after the audio signal no longer contains speech (0.0 to 20x model window length in seconds).

## 0.11.0 - 2025-12-11

#### Features

- **Quail Voice Focus STT** (`Quail_VF_STT_L16`): Purpose-built to isolate and elevate the foreground speaker while suppressing both interfering speech and background noise.
- **New Quail STT model variants**: Added `Quail_STT_L8`, `Quail_STT_S16`, and `Quail_STT_S8` to provide additional model size and sample rate options.
- **Sequential channel processing**: Added `process_sequential` method for processing sequential channel data in a single buffer.


#### Breaking Changes

- **Renamed `Quail_STT` to `Quail_STT_L16`**: The original Quail STT model type has been renamed for consistency with the new model variants.
- **Changed `AicVad::create` signature**: The `model` parameter is no longer `const`.

#### Fixes

- **VAD compatibility with enhancement bypass**: VAD now works correctly when `EnhancementLevel` is set to 0 or `Bypass` is enabled (previously non-functional in these cases).

## 0.10.0 - 2025-11-21

#### Features

- **Quail STT** (`ModelType::Quail_STT`): Our newest speech enhancement model is optimized for human-to-machine interaction (e.g., voice agents, speech-to-text). This model operates at a native sample rate of 16 kHz and uses fixed enhancement parameters that cannot be changed during runtime. The model is also compatible with our VAD.

#### Breaking Changes

- Removed **EnhancementParameter::NoiseGateEnable** as it is now a fixed part of our VAD.
- Added new error code **ErrorCode::ParameterFixed** returned when attempting to modify a parameter of a model with fixed parameters.
- Simplified `AicVad::is_speech_detected()` to return `bool` directly instead of using an output parameter with error code.

#### Fixes

- Fixed an issue where `aic_vad_is_speech_detected` always returned `true` when `VadParameter::LookbackBufferSize` was set to `1.0`.

## 0.9.1 - 2025-11-17

#### New Features

- **Internal library patching**: Static libraries are now patched internally to simplify usage from Rust, reducing integration complexity
- **Windows ARM64 support**: Added Windows ARM64 as a supported target platform

## 0.9.0 - 2025-11-11

#### New Features

##### Voice Activity Detection (VAD) Support
- **Added `AicVad` class** - New C++ wrapper for Voice Activity Detection functionality
  - `AicVad::create()` - Creates a VAD instance from an existing AicModel
  - `is_speech_detected()` - Returns speech detection prediction with latency equal to model's processing latency
  - `set_parameter()` - Configures VAD parameters (LookbackBufferSize, Sensitivity)
  - `get_parameter()` - Retrieves current VAD parameter values
- **Added `VadParameter` enum** - Defines configurable VAD parameters
  - `LookbackBufferSize` - Controls lookback buffer size (1.0-20.0)
  - `Sensitivity` - Controls energy threshold (1.0-15.0)

#### Breaking Changes

- **Renamed `Parameter` enum to `EnhancementParameter`** - More descriptive naming to distinguish from VAD parameters

## 0.8.0 - 2025-10-28

### Features
- **Self-Service Licenses**: Starting with this release, you can use self-service licenses directly from our development portal.
- **Usage-Based Telemetry**: This release introduces a new telemetry feature that collects usage data, paving the way for future usage-based pricing models such as pay-per-minute billing.
  - **What we collect**: We collect only the processing time used and some diagnostic data
  - **Privacy**: We do not collect any information about your audio content. Your audio never leaves your device during our processing.
  - **Requirements**: Requires a constant internet connection. If the SDK cannot be activated online, enhancement will stop after 10 seconds. If telemetry data cannot be sent, enhancement will stop after 5 minutes. When enhancement is stopped an error will be returned, the audio will be bypassed and the processing delay will be still applied to ensure an uninterrupted audio stream without discontinuities.
  - **Error Handling**: When processing is bypassed because our backend cannot be reached or does not allow you to process, the process functions will return `ErrorCode::EnhancementNotAllowed`. Make sure to handle this error code in your implementation.
  - **Offline Licenses**: If you cannot provide a constant internet connection, please contact us to obtain a special offline license that does not require telemetry.

### Breaking Changes
- **Updated Error Codes**: Renumbered and expanded error codes with additional license-related errors.

#### Old Error Codes
| Error Code | Value |
|---|---|
| `ErrorCode::Success` | 0 |
| `ErrorCode::NullPointer` | 1 |
| `ErrorCode::LicenseInvalid` | 2 |
| `ErrorCode::LicenseExpired` | 3 |
| `ErrorCode::UnsupportedAudioConfig` | 4 |
| `ErrorCode::AudioConfigMismatch` | 5 |
| `ErrorCode::NotInitialized` | 6 |
| `ErrorCode::ParameterOutOfRange` | 7 |
| `ErrorCode::ActivationError` | 8 |

#### New Error Codes
| Error Code | Value | Notes |
|---|---|---|
| `ErrorCode::Success` | 0 | Unchanged |
| `ErrorCode::NullPointer` | 1 | Unchanged |
| `ErrorCode::ParameterOutOfRange` | 2 | Renumbered from 7 |
| `ErrorCode::ModelNotInitialized` | 3 | Renamed from `ErrorCode::NotInitialized`, renumbered from 6 |
| `ErrorCode::AudioConfigUnsupported` | 4 | Renamed from `ErrorCode::UnsupportedAudioConfig` |
| `ErrorCode::AudioConfigMismatch` | 5 | Unchanged |
| `ErrorCode::EnhancementNotAllowed` | 6 | **New.** SDK key was not authorized or process failed to report usage. Check if you have internet connection. |
| `ErrorCode::InternalError` | 7 | **New.** Internal error occurred. Contact support. |
| `ErrorCode::LicenseFormatInvalid` | 50 | Renamed from `ErrorCode::LicenseInvalid`, renumbered from 2 |
| `ErrorCode::LicenseVersionUnsupported` | 51 | **New.** License version is not compatible with the SDK version. Update SDK or contact support. |
| `ErrorCode::LicenseExpired` | 52 | Renumbered from 3 |

**Removed:** `ErrorCode::ActivationError` has been removed and split into specific license errors.

### Fixes
- Fixed an issue where, after a successful initialization, a subsequent initialization error would not properly block processing, potentially allowing operations on a partially initialized model.
- Fixed an issue where toggling bypass mode or switching enhancement levels could produce discontinuities.

## 0.7.0 - 2025-10-14

#### Breaking Changes

- **Variable number of frames supported**: The process functions now support a variable number of frames per call. To enable this feature, use the new `allow_variable_frames` parameter in the `initialize` function:
    ```cpp
    ErrorCode initialize(uint32_t sample_rate, uint16_t num_channels, size_t num_frames,
                         bool allow_variable_frames);
    ```
  Set `allow_variable_frames` to `true` to enable variable frame processing, or `false` to maintain the previous fixed frame behavior. Note that enabling variable frames results in higher processing delay.
- **New bypass parameter**: A new parameter `Bypass` has been added to control audio processing bypass while preserving algorithmic delay. When enabled, the input audio passes through unmodified, but the      output is still delayed by the same amount as during normal processing. This ensures seamless transitions when toggling enhancement on/off without audible clicks or timing shifts.
- **Sample rate parameter added to `get_optimal_num_frames`**: The function now takes `sample_rate` as an argument to make the dependency between sample rate and optimal frame count more explicit:
    ```cpp
    size_t get_optimal_num_frames(uint32_t sample_rate);
    ```

#### Fixes

- **Model state reset during pause**: The internal model state is now automatically reset when processing is paused (e.g., when bypass is enabled or enhancement level is set to 0). This ensures a clean state when processing resumes.
- **`reset` now resets all DSP components**: The reset operation now ensures that all internal DSP components are properly reset, providing a more thorough clean state.

## 0.6.3 - 2025-08-21

#### Updates

- **Updated low-sample rate models**: 8- and 16 KHz Quail models updated with improved speech enhancement performance.
