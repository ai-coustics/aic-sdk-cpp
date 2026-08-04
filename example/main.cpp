#include "aic.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

// Runs a dedicated VAD model side by side with the enhancement processor.
//
// The VAD is a standalone object in SDK 0.22 and later: it needs its own model, its own
// initialization and its own process call. It reads the audio block without modifying it, so both
// objects can be fed the same original input buffer.
static int run_vad(const std::string& model_path, const std::string& license_key)
{
    auto model_result = aic::Model::create_from_file(model_path);
    if (!model_result.ok())
    {
        std::cerr << "VAD model creation failed with error code: "
                  << static_cast<int>(model_result.error) << "\n";
        return 1;
    }

    auto model = model_result.take();

    auto vad_result = aic::Vad::create(model, license_key);
    if (!vad_result.ok())
    {
        if (vad_result.error == aic::ErrorCode::ModelTypeUnsupported)
        {
            std::cerr << "Model is not a dedicated VAD model\n";
        }
        else
        {
            std::cerr << "VAD creation failed with error code: "
                      << static_cast<int>(vad_result.error) << "\n";
        }
        return 1;
    }

    auto vad = vad_result.take();

    // The VAD has its own optimal audio configuration, independent of the enhancement model.
    auto sample_rate = model.get_optimal_sample_rate();
    auto block_size  = model.get_optimal_block_size(sample_rate);
    aic::ProcessorConfig config(sample_rate, block_size); // fixed block size

    auto err = vad.initialize(config.sample_rate, config.block_size, config.variable_block_size);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "VAD initialization failed\n";
        return 1;
    }

    auto vad_ctx_result = vad.create_context();
    if (!vad_ctx_result.ok())
    {
        std::cerr << "VAD context creation failed\n";
        return 1;
    }

    auto vad_ctx = vad_ctx_result.take();

    err = vad_ctx.set_parameter(aic::VadParameter::SpeechHoldDuration, 0.1f);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Failed to set VAD speech hold duration\n";
        return 1;
    }

    // Sensitivity is always a probability threshold between 0.0 and 1.0.
    err = vad_ctx.set_parameter(aic::VadParameter::Sensitivity, 0.8f);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Failed to set VAD sensitivity\n";
        return 1;
    }

    std::cout << "VAD speech hold duration: "
              << vad_ctx.get_parameter(aic::VadParameter::SpeechHoldDuration) << "\n";
    std::cout << "VAD sensitivity: " << vad_ctx.get_parameter(aic::VadParameter::Sensitivity)
              << "\n";
    std::cout << "VAD prediction delay: " << vad_ctx.get_prediction_delay() << " samples\n";

    // The VAD does not modify its input, so it can read the same buffer the processor enhances.
    auto audio = std::vector<float>(config.block_size, 0.1f);

    err = vad.process(audio.data(), audio.size());
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "VAD processing failed with error code: " << static_cast<int>(err) << "\n";
        return 1;
    }

    std::cout << "Speech detected: " << (vad_ctx.is_speech_detected() ? "yes" : "no") << "\n";
    std::cout << "Raw VAD probability: " << vad_ctx.get_raw_vad_probability() << "\n";

    err = vad_ctx.reset();
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "VAD reset failed\n";
        return 1;
    }

    return 0;
}

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
        std::cerr << "Error: Provide an enhancement model path as argv[1] and, optionally, a VAD "
                     "model path as argv[2]: `./my_app <model_path> [vad_model_path]`\n";
        return 1;
    }

    // Optional: a dedicated VAD model. Voice activity detection no longer comes from the
    // enhancement processor, so it needs a VAD model of its own.
    auto vad_model_path = std::string();
    if (argc > 2 && argv[2] != nullptr)
    {
        vad_model_path = argv[2];
    }

    auto model_result = aic::Model::create_from_file(model_path);
    auto err          = model_result.error;

    if (!model_result.ok())
    {
        std::cerr << "Model creation failed with error code: " << static_cast<int>(err) << "\n";
        return 1;
    }

    auto model = model_result.take();

    // Query optimal settings from the model
    auto sample_rate = model.get_optimal_sample_rate();
    auto block_size  = model.get_optimal_block_size(sample_rate);

    // Create configuration with optimal settings
    aic::ProcessorConfig config(sample_rate, block_size); // fixed block size

    auto processor_result = aic::Processor::create(model, license_key);
    err                   = processor_result.error;

    if (!processor_result.ok())
    {
        std::cerr << "Processor creation failed with error code: " << static_cast<int>(err) << "\n";
        return 1;
    }

    auto processor = processor_result.take();
    err = processor.initialize(config.sample_rate, config.block_size, config.variable_block_size);
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

    auto ctx = ctx_result.take();
    std::cout << "Audio delay: " << ctx.get_audio_delay() << " samples\n";

    err = ctx.set_parameter(aic::ProcessorParameter::EnhancementLevel, 0.8f);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Failed to set enhancement level\n";
        return 1;
    }

    // All audio APIs are mono. Downmix multi-channel audio before processing, or create one
    // processor per channel.
    auto audio = std::vector<float>(config.block_size, 0.1f);

    err = processor.process(audio.data(), audio.size());
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Processing failed with error code: " << static_cast<int>(err) << "\n";
        return 1;
    }

    auto enhancement_level = ctx.get_parameter(aic::ProcessorParameter::EnhancementLevel);
    std::cout << "Enhancement level: " << enhancement_level << "\n";

    err = ctx.reset();
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Reset failed\n";
        return 1;
    }

    if (!vad_model_path.empty())
    {
        if (run_vad(vad_model_path, license_key) != 0)
        {
            return 1;
        }
    }
    else
    {
        std::cout << "No VAD model path given, skipping the VAD example.\n";
    }

    return 0;
}
