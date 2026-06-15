## New Features

- Added support for OpenTelemetry observability. Pass an `aic::OtelConfig` to `Processor::create` to enable telemetry for a specific session, or pass `nullptr` and set the `AIC_SDK_OTEL_ENABLE` environment variable to enable it globally. The metric export interval is configurable via `OtelConfig::export_interval_ms` (0 keeps the default of 60 000 ms).
- Added support for JWT license keys. Pass a JWT string as the license key to `Processor::create`, then use `ProcessorContext::update_bearer_token` to swap in a renewed token while audio processing continues uninterrupted. In-place updates require both the original and the new key to be JWTs; otherwise the call returns the new `ErrorCode::TokenUpdateUnsupported` and the existing token stays in use.
- Added support for dedicated voice activity detection models, which output a per-buffer speech probability. `VadParameter::Sensitivity` is now interpreted based on the active model: 0.0 to 1.0 for VAD models (the probability threshold above which speech is reported), or 1.0 to 15.0 for energy-based VADs (energy threshold = 10 ^ (-sensitivity)). The default sensitivity is now model-specific.

## Breaking Changes

- Compatible model file version was bumped to 4. Models built for earlier versions are no longer supported.
