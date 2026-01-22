#include "aic.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

int main(int argc, char** argv)
{
    std::cout << "ai-coustics SDK version: " << aic::get_sdk_version() << "\n";
    std::cout << "Compatible model version: " << aic::get_compatible_model_version() << "\n";

    auto license_env = std::getenv("AIC_SDK_LICENSE");
    if (!license_env || std::string(license_env).empty())
    {
        std::cerr << "Error: Environment variable AIC_SDK_LICENSE not set.\n";
        return 1;
    }
    auto license_key = std::string(license_env);

    auto model_path = std::string();
    if (argc > 1 && argv[1] != nullptr)
    {
        model_path = argv[1];
    }

    if (model_path.empty())
    {
        std::cerr << "Error: Provide model path as argv[1]: `./my_app <model_path>`\n";
        return 1;
    }

    auto model_result = aic::Model::create_from_file(model_path);
    auto err          = model_result.error;

    if (!model_result.ok())
    {
        std::cerr << "Model creation failed with error code: " << static_cast<int>(err) << "\n";
        return 1;
    }

    auto model  = model_result.take();

    // Query optimal settings from the model
    auto sample_rate = model.get_optimal_sample_rate();
    auto num_frames = model.get_optimal_num_frames(sample_rate);

    // Create configuration with optimal settings
    aic::ProcessorConfig config(sample_rate, num_frames);  // mono, fixed frames

    auto processor_result = aic::Processor::create(model, license_key);
    err                   = processor_result.error;

    if (!processor_result.ok())
    {
        std::cerr << "Processor creation failed with error code: " << static_cast<int>(err) << "\n";
        return 1;
    }

    auto processor = processor_result.take();
    err = processor.initialize(config.sample_rate, config.num_channels, config.num_frames,
                               config.allow_variable_frames);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Initialization failed\n";
        return 1;
    }

    auto ctx_result = processor.create_context();
    if (!ctx_result.ok())
    {
        std::cerr << "Processor context creation failed\n";
        return 1;
    }

    auto ctx          = ctx_result.take();
    auto output_delay = ctx.get_output_delay();
    std::cout << "Output delay: " << output_delay << " samples\n";

    auto vad_result = processor.create_vad_context();
    if (!vad_result.ok())
    {
        std::cerr << "VAD context creation failed\n";
        return 1;
    }

    auto vad = vad_result.take();
    err      = vad.set_parameter(aic::VadParameter::SpeechHoldDuration, 0.1f);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Failed to set VAD speech hold duration\n";
        return 1;
    }

    err = vad.set_parameter(aic::VadParameter::Sensitivity, 8.0f);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Failed to set VAD sensitivity\n";
        return 1;
    }

    auto speech_hold_duration = vad.get_parameter(aic::VadParameter::SpeechHoldDuration);
    auto sensitivity          = vad.get_parameter(aic::VadParameter::Sensitivity);
    std::cout << "VAD speech hold duration: " << speech_hold_duration << "\n";
    std::cout << "VAD sensitivity: " << sensitivity << "\n";

    err = ctx.set_parameter(aic::ProcessorParameter::EnhancementLevel, 0.8f);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Failed to set enhancement level\n";
        return 1;
    }

    err = ctx.set_parameter(aic::ProcessorParameter::VoiceGain, 1.2f);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Failed to set voice gain\n";
        return 1;
    }

    auto interleaved_buffer = std::vector<float>(config.num_frames * config.num_channels, 0.1f);

    err = processor.process_interleaved(interleaved_buffer.data(), config.num_channels,
                                        config.num_frames);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Interleaved processing failed\n";
        return 1;
    }

    auto planar_buffers = std::vector<std::vector<float>>(
        config.num_channels, std::vector<float>(config.num_frames, 0.1f));
    auto channel_ptrs = std::vector<float*>(config.num_channels);
    for (uint16_t i = 0; i < config.num_channels; ++i)
    {
        channel_ptrs[i] = planar_buffers[i].data();
    }

    err = processor.process_planar(channel_ptrs.data(), config.num_channels, config.num_frames);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Planar processing failed\n";
        return 1;
    }

    auto sequential_buffer = std::vector<float>(config.num_frames * config.num_channels, 0.1f);
    err = processor.process_sequential(sequential_buffer.data(), config.num_channels,
                                       config.num_frames);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Sequential processing failed\n";
        return 1;
    }

    auto speech_detected = vad.is_speech_detected();
    std::cout << "Speech detected: " << (speech_detected ? "yes" : "no") << "\n";

    auto enhancement_level = ctx.get_parameter(aic::ProcessorParameter::EnhancementLevel);
    auto voice_gain        = ctx.get_parameter(aic::ProcessorParameter::VoiceGain);

    std::cout << "Enhancement level: " << enhancement_level << "\n";
    std::cout << "Voice gain: " << voice_gain << "\n";

    err = ctx.reset();
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Reset failed\n";
        return 1;
    }

    return 0;
}
