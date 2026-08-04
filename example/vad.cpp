// Voice activity detection example.
//
// The VAD is a standalone object with its own lifecycle: it needs a dedicated VAD model, its own
// initialization, and its own process call. It never modifies the audio it reads.
//
// To run enhancement and VAD together, pass the same input block to both objects, calling
// Vad::process first so the VAD sees the unprocessed signal. See enhancement.cpp for the
// Processor side.

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
        std::cerr << "Usage: " << argv[0] << " <path/to/vad-model.aicmodel>\n";
        return 1;
    }

    auto model_path = std::string(argv[1]);
    std::cout << "Using VAD model: " << model_path << "\n";

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

    // Only dedicated VAD models are accepted. Enhancement models are rejected with
    // ErrorCode::ModelTypeUnsupported; there is no energy-based VAD any more.
    auto vad_result = aic::Vad::create(model, license_key);
    if (!vad_result.ok())
    {
        if (vad_result.error == aic::ErrorCode::ModelTypeUnsupported)
        {
            std::cerr << "Model is not a dedicated VAD model\n";
        }
        else if (!print_error(vad_result.error))
        {
            std::cerr << "VAD creation failed with error: " << static_cast<int>(vad_result.error)
                      << "\n";
        }
        return 1;
    }
    auto vad = vad_result.take();

    // The VAD model has its own optimal configuration, independent of any enhancement model.
    auto sample_rate = model.get_optimal_sample_rate();
    auto block_size  = model.get_optimal_block_size(sample_rate);
    std::cout << "Optimal sample rate: " << sample_rate << " Hz\n";
    std::cout << "Optimal block size: " << block_size << "\n";

    auto err = vad.initialize(sample_rate, block_size, false);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "VAD initialization failed with error: " << static_cast<int>(err) << "\n";
        return 1;
    }

    // The context carries the thread-safe control and query APIs and can be used while audio is
    // running.
    auto context_result = vad.create_context();
    if (!context_result.ok())
    {
        std::cerr << "VAD context creation failed with error: "
                  << static_cast<int>(context_result.error) << "\n";
        return 1;
    }
    auto context = context_result.take();

    // Sensitivity is a probability threshold: the model reports a speech probability per block,
    // and a value above the threshold triggers a speech detected decision.
    if (context.set_parameter(aic::VadParameter::Sensitivity, 0.8f) == aic::ErrorCode::Success)
    {
        std::cout << "VAD sensitivity: " << context.get_parameter(aic::VadParameter::Sensitivity)
                  << "\n";
    }

    if (context.set_parameter(aic::VadParameter::SpeechHoldDuration, 0.05f) ==
        aic::ErrorCode::Success)
    {
        // The duration is rounded to the model's window length, so it may read back differently.
        std::cout << "VAD speech hold duration: "
                  << context.get_parameter(aic::VadParameter::SpeechHoldDuration) << "\n";
    }

    // How far behind its input the published prediction is. This delay is not applied to the
    // audio, the VAD never modifies the buffer.
    std::cout << "VAD prediction delay: " << context.get_prediction_delay() << " samples\n";

    // All audio APIs are mono. The VAD reads the block without modifying it, so the same buffer
    // can be handed to a Processor afterwards.
    auto audio = std::vector<float>(block_size, 0.0f);

    err = vad.process(audio.data(), audio.size());
    if (err != aic::ErrorCode::Success)
    {
        if (!print_error(err))
        {
            std::cerr << "VAD processing failed with error: " << static_cast<int>(err) << "\n";
        }
        return 1;
    }
    std::cout << "VAD processing succeeded\n";

    std::cout << "Speech detected: " << (context.is_speech_detected() ? "yes" : "no") << "\n";

    // The model's direct output, without the SDK's post-processing.
    std::cout << "Raw VAD probability: " << context.get_raw_vad_probability() << "\n";

    // Clears the buffers and drops the published prediction, so queries cannot return stale
    // values from the previous stream. The VAD stays initialized.
    if (context.reset() == aic::ErrorCode::Success)
    {
        std::cout << "VAD reset succeeded\n";
    }

    std::cout << "VAD example completed\n";
    return 0;
}
