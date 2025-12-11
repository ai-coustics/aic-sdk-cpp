# Release Notes

### Features

- **Quail Voice Focus STT** (`Quail_VF_STT_L16`): Purpose-built to isolate and elevate the foreground speaker while suppressing both interfering speech and background noise.
- **New Quail STT model variants**: Added `Quail_STT_L8`, `Quail_STT_S16`, and `Quail_STT_S8` to provide additional model size and sample rate options.
- **Sequential channel processing**: Added `process_sequential` method for processing sequential channel data in a single buffer.


### Breaking Changes

- **Renamed `Quail_STT` to `Quail_STT_L16`**: The original Quail STT model type has been renamed for consistency with the new model variants.
- **Changed `AicVad::create` signature**: The `model` parameter is no longer `const`.

### Fixes

- **VAD compatibility with enhancement bypass**: VAD now works correctly when `EnhancementLevel` is set to 0 or `Bypass` is enabled (previously non-functional in these cases).
