# Release Notes

### Features

- **Quail Voice Focus STT** (`AIC_MODEL_TYPE_QUAIL_VF_STT_L16`): Purpose-built to isolate and elevate the foreground speaker while suppressing both interfering speech and background noise.
- **New Quail STT model variants**: Added `AIC_MODEL_TYPE_QUAIL_STT_L8`, `AIC_MODEL_TYPE_QUAIL_STT_S16`, and `AIC_MODEL_TYPE_QUAIL_STT_S8` to provide additional model size and sample rate options.
- **Sequential channel processing**: Added `aic_model_process_sequential` function for processing sequential channel data in a single buffer.


### Breaking Changes

- **Renamed `AIC_MODEL_TYPE_QUAIL_STT` to `AIC_MODEL_TYPE_QUAIL_STT_L16`**: The original Quail STT model type has been renamed for consistency with the new model variants.
- **Changed `aic_vad_create` signature**: The `model` parameter is no longer `const`.

### Fixes

- **VAD compatibility with enhancement bypass**: VAD now works correctly when `AIC_ENHANCEMENT_PARAMETER_ENHANCEMENT_LEVEL` is set to 0 or `AIC_ENHANCEMENT_PARAMETER_BYPASS` is enabled (previously non-functional in these cases).
