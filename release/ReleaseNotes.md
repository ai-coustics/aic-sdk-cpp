## New Features

This release adds support for our newest audio intelligence model, *Tyto*, through two new types: `Collector` and `Analyzer`.

- The `Collector` is placed in the audio thread and buffers audio chunks for later analysis. Initialize it with the same configuration as your `Processor` and feed it audio with the `buffer_planar` / `buffer_interleaved` / `buffer_sequential` methods, mirroring the `Processor` process methods. The input audio is read-only and is not modified.
- The `Analyzer` runs separately, since analysis models are too expensive for the audio thread. Call `analyze_buffered` from another thread to obtain an `AnalysisResult` for the latest buffered audio.

Create the pair with `AnalyzerPair::create(model, license_key)`, then move the `collector` and `analyzer` to their respective threads. A JWT token can be refreshed on a running analyzer with `Analyzer::update_bearer_token`.

## Breaking Changes

- Compatible model file version was bumped to 5. Models built for earlier versions are no longer supported.
