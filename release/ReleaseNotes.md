This release contains two breaking changes that affect every integration:

- **All audio APIs are mono only.** The multi-channel process/buffer methods are gone.
- **The VAD is its own object.** VAD runs on dedicated VAD models through the new `aic::Vad` class,
  and can no longer be derived from a `Processor`.

Both migrations are covered step by step below.

## Breaking Changes

### Multi-channel support removed

`Processor` and the analyzer's `Collector` now operate on mono audio only.

All models process mono inputs. Previously the processor mixed all input channels down to mono
internally, which could lead to surprising results. To prevent misunderstandings, all APIs now take
exclusively mono inputs.

To process multi-channel audio, downmix to mono before calling `Processor::process`, or create a
separate `Processor` instance per channel.

#### What changed

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

#### Before

```cpp
auto sample_rate = model.get_optimal_sample_rate();
auto num_frames  = model.get_optimal_num_frames(sample_rate);

// Stereo in, stereo out. The SDK mixed both channels down to mono internally.
processor.initialize(sample_rate, 2, num_frames, false);

std::vector<float*> planar = {left.data(), right.data()};
processor.process_planar(planar.data(), 2, num_frames);
```

#### After

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

### VAD moved into its own object, energy-based VAD removed

Voice activity detection is no longer a side effect of enhancement. It is now a first-class type,
`aic::Vad`, that runs a dedicated VAD model.

Energy-based VADs, which inferred speech activity from the output level of an enhancement model,
have been removed. They were an approximation and their accuracy depended on the enhancement model
in use. A dedicated VAD model is trained for the task and is considerably more accurate.

#### What changed

| Before (0.21.0) | Now |
| --- | --- |
| VAD came from a processor: `processor.create_vad_context()` | VAD is standalone: `aic::Vad::create` → `vad.initialize` → `vad.process`, then `vad.create_context()` |
| Any enhancement model provided a VAD (energy-based), VAD models were also loaded into a `Processor` | Only dedicated VAD models are accepted by `aic::Vad::create`; `aic::Processor::create` accepts only enhancement and bypass models |
| VAD advanced whenever the processor processed audio | VAD advances on `Vad::process`, independently of any processor |
| `VadParameter::Sensitivity` ranged 0.0 - 1.0 on VAD models and 1.0 - 15.0 on energy-based VADs | `VadParameter::Sensitivity` is always a probability threshold, 0.0 - 1.0 |

#### Before

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

#### After

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

#### Run the VAD on the original audio

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

#### Delay queries renamed

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

### Renamed error codes

Some error codes are no longer processor-specific, since they are now also returned by the VAD:

| Before (0.21.0) | Now |
| --- | --- |
| `ErrorCode::ProcessorNotInitialized` | `ErrorCode::NotInitialized` |
| `ErrorCode::EnhancementNotAllowed` | `ErrorCode::ProcessingNotAllowed` |
| `ErrorCode::ModelFilePathInvalid` | `ErrorCode::FilePathInvalid` |

The numeric values are unchanged, so only source-level references need updating.

## New Features

There are new explicit session termination APIs. Use these to close telemetry sessions during
lifecycle events without waiting for the corresponding object to be destroyed, which is useful in
integrations where destruction may be delayed.

- `Processor::terminate_session`
- `Vad::terminate_session`
- `Analyzer::terminate_session`

## Bug Fixes

- Resetting VAD state now immediately clears the published speech detection and raw VAD
  probability values, so `VadContext::is_speech_detected` and
  `VadContext::get_raw_vad_probability` no longer return stale values from the previous stream
  after `VadContext::reset`.
