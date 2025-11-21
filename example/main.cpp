#include "aic.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

int main()
{
    const char* license_env = std::getenv("AIC_SDK_LICENSE");
    if (!license_env || std::string(license_env).empty())
    {
        std::cerr << "Error: Environment variable AIC_SDK_LICENSE not set.\n";
        return 1;
    }
    std::string license_key(license_env);

    auto creation_result = aic::AicModel::create(aic::ModelType::Quail_L48, license_key);
    std::unique_ptr<aic::AicModel>& model = creation_result.first;
    aic::ErrorCode                  err   = creation_result.second;

    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Model creation failed with error code: " << static_cast<int>(err) << "\n";
        return 1;
    }

    uint32_t sample_rate = model->get_optimal_sample_rate();
    size_t   num_frames  = model->get_optimal_num_frames(sample_rate);
    std::cout << "Optimal sample rate: " << sample_rate << " Hz\n";
    std::cout << "Optimal number of frames: " << num_frames << "\n";
    uint16_t num_chans = 1;

    err = model->initialize(sample_rate, num_chans, num_frames, false);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Initialization failed\n";
        return 1;
    }

    size_t output_delay = model->get_output_delay();
    std::cout << "Output delay: " << output_delay << " samples\n";

    // Create and test VAD
    auto vad_creation_result = aic::AicVad::create(*model);
    std::unique_ptr<aic::AicVad>& vad = vad_creation_result.first;
    aic::ErrorCode                vad_err = vad_creation_result.second;

    if (vad_err != aic::ErrorCode::Success)
    {
        std::cerr << "VAD creation failed with error code: " << static_cast<int>(vad_err) << "\n";
        return 1;
    }

    // Set VAD parameters
    vad_err = vad->set_parameter(aic::VadParameter::LookbackBufferSize, 5.0f);
    if (vad_err != aic::ErrorCode::Success)
    {
        std::cerr << "Failed to set VAD lookback buffer size\n";
        return 1;
    }

    vad_err = vad->set_parameter(aic::VadParameter::Sensitivity, 8.0f);
    if (vad_err != aic::ErrorCode::Success)
    {
        std::cerr << "Failed to set VAD sensitivity\n";
        return 1;
    }

    // Get VAD parameter values
    float lookback = vad->get_parameter(aic::VadParameter::LookbackBufferSize);
    float sensitivity = vad->get_parameter(aic::VadParameter::Sensitivity);
    std::cout << "VAD lookback buffer size: " << lookback << "\n";
    std::cout << "VAD sensitivity: " << sensitivity << "\n";

    // Test all available parameters
    err = model->set_parameter(aic::EnhancementParameter::EnhancementLevel, 0.8f);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Failed to set enhancement level\n";
        return 1;
    }

    err = model->set_parameter(aic::EnhancementParameter::VoiceGain, 1.2f);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Failed to set voice gain\n";
        return 1;
    }

    // Test both interleaved and planar processing
    std::vector<float> buffer(num_frames * num_chans, 0.1f);

    err = model->process_interleaved(buffer.data(), num_chans, num_frames);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Interleaved processing failed\n";
        return 1;
    }

    // Test planar processing
    std::vector<float*> channel_ptrs(num_chans);
    for (uint16_t i = 0; i < num_chans; ++i)
    {
        channel_ptrs[i] = buffer.data() + (i * num_frames);
    }

    err = model->process_planar(channel_ptrs.data(), num_chans, num_frames);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Planar processing failed\n";
        return 1;
    }

    // Check if speech is detected after processing
    bool speech_detected = vad->is_speech_detected();
    if (vad_err != aic::ErrorCode::Success)
    {
        std::cerr << "Failed to check speech detection\n";
        return 1;
    }
    std::cout << "Speech detected: " << (speech_detected ? "yes" : "no") << "\n";

    // Get all parameter values to verify they were set correctly
    float enhancement_level = model->get_parameter(aic::EnhancementParameter::EnhancementLevel);
    float voice_gain        = model->get_parameter(aic::EnhancementParameter::VoiceGain);

    std::cout << "Enhancement level: " << enhancement_level << "\n";
    std::cout << "Voice gain: " << voice_gain << "\n";

    model->reset();

    std::cout << "ai-coustics SDK version: " << aic::AicModel::get_sdk_version() << "\n";
    return 0;
}
