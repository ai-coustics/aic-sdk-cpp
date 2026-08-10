## Breaking Changes

### New model file version

This release requires model file version 7. Re-download your models so the SDK does not reject them
with `ErrorCode::ModelVersionUnsupported`. See the
[compatibility matrix](https://docs.ai-coustics.com/reference/sdk/compatibility-matrix).

Model files are available at [artifacts.ai-coustics.io](https://artifacts.ai-coustics.io/), and the
version this SDK build expects can be queried at runtime:

```cpp
std::cout << "Compatible model version: " << aic::get_compatible_model_version() << "\n";
```

## New Features

### Tyto 1.0 has been replaced by Tyto 1.1

The analysis model is now Tyto 1.1, `tyto-1.1-l-16khz`. Tyto 1.0 (`tyto-l-16khz`) is not loadable by
this SDK version any more: it has no model file version 7 release, so
`aic::Model::create_from_file` rejects it with `ErrorCode::ModelVersionUnsupported`.

### Analysis result fields changed

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

## Bug Fixes

- `Analyzer::analyze_buffered` no longer crashes when OpenTelemetry reporting is enabled.
