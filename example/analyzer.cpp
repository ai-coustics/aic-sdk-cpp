// Audio analysis example.
//
// Analysis runs on a Collector / Analyzer pair created together from a dedicated analysis model.
// The Collector belongs in the audio thread and only buffers; the Analyzer runs the model from
// another thread, because analysis models are far too expensive for a real-time callback.
//
// Enhancement and voice activity detection are separate objects with their own models; see
// enhancement.cpp and vad.cpp.

#include "aic.hpp"

#include <cstdlib>
#include <iomanip>
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
        std::cerr << "Usage: " << argv[0] << " <path/to/analysis-model.aicmodel>\n";
        return 1;
    }

    auto model_path = std::string(argv[1]);
    std::cout << "Using analysis model: " << model_path << "\n";

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

    // Only dedicated analysis models are accepted. An enhancement or VAD model is rejected with
    // ErrorCode::ModelTypeUnsupported.
    auto pair_result = aic::AnalyzerPair::create(model, license_key);
    if (!pair_result.ok())
    {
        if (pair_result.error == aic::ErrorCode::ModelTypeUnsupported)
        {
            std::cerr << "Model is not an analysis model\n";
        }
        else if (!print_error(pair_result.error))
        {
            std::cerr << "Analyzer creation failed with error: "
                      << static_cast<int>(pair_result.error) << "\n";
        }
        return 1;
    }
    auto pair = pair_result.take();

    // The two halves are independent and can be moved where they belong: the collector into the
    // audio thread, the analyzer onto a thread of your own.
    auto collector = std::move(pair.collector);
    auto analyzer  = std::move(pair.analyzer);

    // The analysis model has its own optimal configuration, independent of any other model.
    auto sample_rate = model.get_optimal_sample_rate();
    auto block_size  = model.get_optimal_block_size(sample_rate);
    std::cout << "Optimal sample rate: " << sample_rate << " Hz\n";
    std::cout << "Optimal block size: " << block_size << "\n";

    // variable_block_size is true here because the last block of the loop below is shorter than
    // block_size whenever the signal length is not a multiple of it.
    auto err = collector.initialize(sample_rate, block_size, true);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Collector initialization failed with error: " << static_cast<int>(err)
                  << "\n";
        return 1;
    }

    // A one-second mono signal to analyze (silence here, for demonstration). All audio APIs are
    // mono: downmix multichannel audio before buffering, or create one pair per channel.
    auto signal = std::vector<float>(sample_rate, 0.0f);

    // In a real integration this loop is your audio callback. Buffering is real-time safe and
    // leaves the input untouched.
    for (size_t offset = 0; offset < signal.size(); offset += block_size)
    {
        auto remaining  = signal.size() - offset;
        auto this_block = remaining < block_size ? remaining : block_size;

        err = collector.buffer(signal.data() + offset, this_block);
        if (err != aic::ErrorCode::Success)
        {
            std::cerr << "Audio buffering failed with error: " << static_cast<int>(err) << "\n";
            return 1;
        }
    }
    std::cout << "Audio buffering succeeded\n";

    // Never call this from the audio thread. The model consumes a fixed length of audio; if the
    // collector holds less than that, the tail is analyzed as silence.
    auto analysis_result = analyzer.analyze_buffered();
    if (!analysis_result.ok())
    {
        if (!print_error(analysis_result.error))
        {
            std::cerr << "Analysis failed with error: " << static_cast<int>(analysis_result.error)
                      << "\n";
        }
        return 1;
    }
    const aic::AnalysisResult& scores = analysis_result.value;

    // Every score ranges from 0.0 to 1.0. For all measures except speaker_loudness, a lower value
    // indicates less problematic audio.
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "risk_score:         " << scores.risk_score << "\n";
    std::cout << "speaker_reverb:     " << scores.speaker_reverb << "\n";
    std::cout << "speaker_loudness:   " << scores.speaker_loudness << "\n";
    std::cout << "interfering_speech: " << scores.interfering_speech << "\n";
    std::cout << "noise:              " << scores.noise << "\n";
    std::cout << "codec_degradation:  " << scores.codec_degradation << "\n";
    std::cout << "packet_loss:        " << scores.packet_loss << "\n";

    // Clears the buffered audio and the state of both halves. The collector stays initialized.
    if (analyzer.reset() == aic::ErrorCode::Success)
    {
        std::cout << "Analyzer reset succeeded\n";
    }

    std::cout << "Analysis example completed\n";
    return 0;
}
