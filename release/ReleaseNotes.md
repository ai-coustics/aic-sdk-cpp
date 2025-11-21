# Release Notes

### Features

- **Quail STT** (`ModelType::Quail_STT`): Our newest speech enhancement model is optimized for human-to-machine interaction (e.g., voice agents, speech-to-text). This model operates at a native sample rate of 16 kHz and uses fixed enhancement parameters that cannot be changed during runtime. The model is also compatible with our VAD.

### Breaking Changes

- Removed **EnhancementParameter::NoiseGateEnable** as it is now a fixed part of our VAD.
- Added new error code **ErrorCode::ParameterFixed** returned when attempting to modify a parameter of a model with fixed parameters.
- Simplified `AicVad::is_speech_detected()` to return `bool` directly instead of using an output parameter with error code.

### Fixes

- Fixed an issue where `aic_vad_is_speech_detected` always returned `true` when `VadParameter::LookbackBufferSize` was set to `1.0`.
