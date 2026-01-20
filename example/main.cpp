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

    const char* license_env = std::getenv("AIC_SDK_LICENSE");
    if (!license_env || std::string(license_env).empty())
    {
        std::cerr << "Error: Environment variable AIC_SDK_LICENSE not set.\n";
        return 1;
    }
    std::string license_key(license_env);

    std::string model_path;
    if (argc > 1 && argv[1] != nullptr)
    {
        model_path = argv[1];
    }
    else
    {
        const char* model_env = std::getenv("AIC_SDK_MODEL_PATH");
        if (model_env && model_env[0] != '\0')
        {
            model_path = model_env;
        }
    }

    if (model_path.empty())
    {
        std::cerr << "Error: Provide model path as argv[1] or set AIC_SDK_MODEL_PATH.\n";
        return 1;
    }

    aic::Result<aic::Model> model_result = aic::Model::create_from_file(model_path);
    aic::ErrorCode          err          = model_result.error;

    if (!model_result.ok())
    {
        std::cerr << "Model creation failed with error code: " << static_cast<int>(err) << "\n";
        return 1;
    }

    aic::Model model = std::move(model_result.value);
    aic::Model& model_ref = model;
    auto config = aic::ProcessorConfig::optimal(model_ref).with_num_channels(1);

    aic::Result<aic::Processor> processor_result = aic::Processor::create(model_ref, license_key);
    err                                          = processor_result.error;

    if (!processor_result.ok())
    {
        std::cerr << "Processor creation failed with error code: " << static_cast<int>(err) << "\n";
        return 1;
    }

    aic::Processor processor = std::move(processor_result.value);
    aic::Processor& processor_ref = processor;
    err = processor_ref.initialize(config.sample_rate, config.num_channels, config.num_frames,
                                   config.allow_variable_frames);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Initialization failed\n";
        return 1;
    }

    aic::Result<aic::ProcessorContext> ctx_result = processor_ref.create_context();
    if (!ctx_result.ok())
    {
        std::cerr << "Processor context creation failed\n";
        return 1;
    }

    aic::ProcessorContext ctx = std::move(ctx_result.value);
    aic::ProcessorContext& ctx_ref = ctx;
    size_t output_delay = ctx_ref.get_output_delay();
    std::cout << "Output delay: " << output_delay << " samples\n";

    aic::Result<aic::VadContext> vad_result = processor_ref.create_vad_context();
    if (!vad_result.ok())
    {
        std::cerr << "VAD context creation failed\n";
        return 1;
    }

    aic::VadContext vad = std::move(vad_result.value);
    aic::VadContext& vad_ref = vad;
    err = vad_ref.set_parameter(aic::VadParameter::SpeechHoldDuration, 0.1f);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Failed to set VAD speech hold duration\n";
        return 1;
    }

    err = vad_ref.set_parameter(aic::VadParameter::Sensitivity, 8.0f);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Failed to set VAD sensitivity\n";
        return 1;
    }

    float speech_hold_duration = vad_ref.get_parameter(aic::VadParameter::SpeechHoldDuration);
    float sensitivity = vad_ref.get_parameter(aic::VadParameter::Sensitivity);
    std::cout << "VAD speech hold duration: " << speech_hold_duration << "\n";
    std::cout << "VAD sensitivity: " << sensitivity << "\n";

    err = ctx_ref.set_parameter(aic::ProcessorParameter::EnhancementLevel, 0.8f);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Failed to set enhancement level\n";
        return 1;
    }

    err = ctx_ref.set_parameter(aic::ProcessorParameter::VoiceGain, 1.2f);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Failed to set voice gain\n";
        return 1;
    }

    std::vector<float> interleaved_buffer(config.num_frames * config.num_channels, 0.1f);

    err = processor_ref.process_interleaved(interleaved_buffer.data(), config.num_channels,
                                            config.num_frames);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Interleaved processing failed\n";
        return 1;
    }

    std::vector<std::vector<float> > planar_buffers(config.num_channels,
                                                     std::vector<float>(config.num_frames, 0.1f));
    std::vector<float*> channel_ptrs(config.num_channels);
    for (uint16_t i = 0; i < config.num_channels; ++i)
    {
        channel_ptrs[i] = planar_buffers[i].data();
    }

    err = processor_ref.process_planar(channel_ptrs.data(), config.num_channels, config.num_frames);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Planar processing failed\n";
        return 1;
    }

    std::vector<float> sequential_buffer(config.num_frames * config.num_channels, 0.1f);
    err = processor_ref.process_sequential(sequential_buffer.data(), config.num_channels,
                                           config.num_frames);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Sequential processing failed\n";
        return 1;
    }

    bool speech_detected = vad_ref.is_speech_detected();
    std::cout << "Speech detected: " << (speech_detected ? "yes" : "no") << "\n";

    float enhancement_level = ctx_ref.get_parameter(aic::ProcessorParameter::EnhancementLevel);
    float voice_gain        = ctx_ref.get_parameter(aic::ProcessorParameter::VoiceGain);

    std::cout << "Enhancement level: " << enhancement_level << "\n";
    std::cout << "Voice gain: " << voice_gain << "\n";

    err = ctx_ref.reset();
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Reset failed\n";
        return 1;
    }

    return 0;
}
