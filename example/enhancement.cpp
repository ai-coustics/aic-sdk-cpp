// Speech enhancement example.
//
// Loads an enhancement or bypass model, runs one mono block through a Processor, and exercises
// the thread-safe control APIs on its context.
//
// Voice activity detection is a separate object with its own model; see vad.cpp.

#include "aic.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

// Reports the errors that need real handling: a bad license, or an authorization failure that can
// appear mid-stream. Returns true when the error was recognized and printed.
static bool print_error(aic::ErrorCode error)
{
    switch (error)
    {
    case aic::ErrorCode::LicenseFormatInvalid:
        std::cerr << "Invalid license key: check AIC_SDK_LICENSE.\n";
        return true;
    case aic::ErrorCode::LicenseVersionUnsupported:
        std::cerr << "Unsupported license version: update the SDK or license key.\n";
        return true;
    case aic::ErrorCode::LicenseExpired:
        std::cerr << "License expired: renew AIC_SDK_LICENSE.\n";
        return true;
    case aic::ErrorCode::ProcessingNotAllowed:
        std::cerr << "Processing not allowed: check AIC_SDK_LICENSE/network.\n";
        return true;
    default:
        return false;
    }
}

int main(int argc, char** argv)
{
    if (argc < 2 || argv[1] == nullptr)
    {
        std::cerr << "Usage: " << argv[0] << " <path/to/model.aicmodel>\n";
        return 1;
    }

    auto model_path = std::string(argv[1]);
    std::cout << "Using model: " << model_path << "\n";

    std::cout << "Library version: " << aic::get_sdk_version() << "\n";
    std::cout << "Compatible model version: " << aic::get_compatible_model_version() << "\n";

    auto license_env = std::getenv("AIC_SDK_LICENSE");
    if (!license_env || std::string(license_env).empty())
    {
        std::cerr << "Error: AIC_SDK_LICENSE environment variable not set\n";
        std::cerr << "Please set it with: export AIC_SDK_LICENSE=your_license_key\n";
        return 1;
    }
    auto license_key = std::string(license_env);

    auto model_result = aic::Model::create_from_file(model_path);
    if (!model_result.ok())
    {
        std::cerr << "Model creation failed with error: " << static_cast<int>(model_result.error)
                  << "\n";
        return 1;
    }
    auto model = model_result.take();

    // The processor accepts enhancement and bypass models. A VAD or analysis model is rejected
    // with ErrorCode::ModelTypeUnsupported.
    auto processor_result = aic::Processor::create(model, license_key);
    if (!processor_result.ok())
    {
        if (processor_result.error == aic::ErrorCode::ModelTypeUnsupported)
        {
            std::cerr << "Model is not an enhancement or bypass model\n";
        }
        else if (!print_error(processor_result.error))
        {
            std::cerr << "Processor creation failed with error: "
                      << static_cast<int>(processor_result.error) << "\n";
        }
        return 1;
    }
    auto processor = processor_result.take();

    // Every model runs at any supported sample rate and block size. The model's own configuration
    // avoids internal resampling and buffering, and therefore gives the lowest delay.
    auto sample_rate = model.get_optimal_sample_rate();
    auto block_size  = model.get_optimal_block_size(sample_rate);
    std::cout << "Optimal sample rate: " << sample_rate << " Hz\n";
    std::cout << "Optimal block size: " << block_size << "\n";

    auto err = processor.initialize(sample_rate, block_size, false);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Processor initialization failed with error: " << static_cast<int>(err)
                  << "\n";
        return 1;
    }

    // All audio APIs are mono. Downmix multichannel audio before processing, or create one
    // processor per channel.
    auto audio = std::vector<float>(block_size, 0.0f);

    err = processor.process(audio.data(), audio.size());
    if (err != aic::ErrorCode::Success)
    {
        if (!print_error(err))
        {
            std::cerr << "Audio processing failed with error: " << static_cast<int>(err) << "\n";
        }
        return 1;
    }
    std::cout << "Audio processing succeeded\n";

    // The context carries the thread-safe control APIs and can be used while audio is running.
    auto context_result = processor.create_context();
    if (!context_result.ok())
    {
        std::cerr << "Processor context creation failed with error: "
                  << static_cast<int>(context_result.error) << "\n";
        return 1;
    }
    auto context = context_result.take();

    if (context.reset() == aic::ErrorCode::Success)
    {
        std::cout << "Processor reset succeeded\n";
    }

    if (context.set_parameter(aic::ProcessorParameter::EnhancementLevel, 0.7f) ==
        aic::ErrorCode::Success)
    {
        std::cout << "Enhancement level: "
                  << context.get_parameter(aic::ProcessorParameter::EnhancementLevel) << "\n";
    }

    // How many samples the enhanced audio lags its input by.
    std::cout << "Audio delay: " << context.get_audio_delay() << " samples\n";

    std::cout << "Enhancement example completed\n";
    return 0;
}
