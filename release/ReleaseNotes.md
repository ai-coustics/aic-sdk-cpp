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

## New features

- Models now load from files via `aic::Model::create_from_file`.
- Models can also be created from in-memory buffers with `aic::Model::create_from_buffer`.
- Added `aic::Model::get_id` to query the id of a model.
- A single model instance can be shared across multiple processors.
- Added `aic::Processor::create` so each stream can be initialized independently from a shared model while sharing weights.
- Added `aic::get_compatible_model_version` to query the required model version for this SDK.
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

## Breaking changes

- License keys previously generated in the [development portal](developers.ai-coustics.io) will no longer work. New license keys have to be generated.
- Existing model-processing combined API has been split into `aic::Model` and `aic::Processor`.
- Removed `aic::ModelType` enum; callers must supply a model file or aligned buffer instead of selecting a built-in model.
- License keys are now provided to `aic::Processor::create` rather than model creation.
- Renamed `aic::Parameter` to `aic::ProcessorParameter`.
- VAD APIs now use `aic::VadContext` created from a processor.
- Processor control APIs now live on `aic::ProcessorContext` (reset, parameter access, output delay).
- Model query APIs now live on `aic::Model` (optimal sample rate/frames).
- C++ wrapper breaking changes:
    - `aic::AicModel` wrapper replaced by `aic::Model` + `aic::Processor`
    - Model selection by enum removed; supply a model file or aligned buffer
    - Parameter/reset/output delay APIs now live on `aic::ProcessorContext`
    - VAD APIs now use `aic::VadContext` created from a processor
    - `std::pair`/`std::unique_ptr` creation patterns replaced by `aic::Result<T>` + `take()`

## Fixes

- Improved thread safety.
- Fixed an issue where the allocated size for an FFT operation could be incorrect, leading to a crash.
