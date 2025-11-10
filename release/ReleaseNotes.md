# Release Notes

### New Features

#### Voice Activity Detection (VAD) Support
- **Added `AicVad` class** - New C++ wrapper for Voice Activity Detection functionality
  - `AicVad::create()` - Creates a VAD instance from an existing AicModel
  - `is_speech_detected()` - Returns speech detection prediction with latency equal to model's processing latency
  - `set_parameter()` - Configures VAD parameters (LookbackBufferSize, Sensitivity)
  - `get_parameter()` - Retrieves current VAD parameter values
- **Added `VadParameter` enum** - Defines configurable VAD parameters
  - `LookbackBufferSize` - Controls lookback buffer size (1.0-20.0)
  - `Sensitivity` - Controls energy threshold (1.0-15.0)

### Breaking Changes

- **Renamed `Parameter` enum to `EnhancementParameter`** - More descriptive naming to distinguish from VAD parameters
