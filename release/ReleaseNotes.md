### Features

- **New VAD parameter `VadParameter::MinimumSpeechDuration`**: Controls for how long speech needs to be present in the audio signal before the VAD considers it speech (0.0 - 1.0 seconds).

### Breaking Changes

- **Replaced VAD parameter `VadParameter::LookbackBufferSize` with `VadParameter::SpeechHoldDuration`**: The new parameter controls for how long the VAD continues to detect speech after the audio signal no longer contains speech (0.0 to 20x model window length in seconds).
