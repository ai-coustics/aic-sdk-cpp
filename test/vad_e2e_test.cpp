// End-to-end VAD test.
//
// Loads an audio file, runs it block by block through a standalone Vad, and compares the
// per-block speech-detected decisions against a pre-generated reference. This mirrors the
// reference test in the C SDK (aic-sdk-interface's `process_vad_blocks`), so the same fixtures
// apply.
//
// The audio is split into the model's optimal block size and any trailing partial block is
// dropped, matching the reference generator exactly so the two stay bit-for-bit comparable.
//
// Usage:
//   aic-sdk-vad-e2e-test <vad-model.aicmodel> <input.wav> <expected.json>
//   aic-sdk-vad-e2e-test --generate <vad-model.aicmodel> <input.wav> <output.json>
//
// The second form regenerates a reference file instead of comparing against one. Use it when a
// VAD model is added or when a deliberate change to the SDK moves the expected decisions.

#include "aic.hpp"
#include "wav_io.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using aic_test::Audio;
using aic_test::load_mono_wav;

namespace
{

// Writes a JSON array of booleans, one per line, matching serde_json::to_string_pretty's default
// two-space indentation so regenerated fixtures diff cleanly against the Rust SDK's.
bool write_bool_array(const std::string& path, const std::vector<bool>& values)
{
    std::ofstream stream(path, std::ios::binary);
    if (!stream)
    {
        std::cerr << "Failed to write results file: " << path << "\n";
        return false;
    }

    stream << "[\n";
    for (size_t i = 0; i < values.size(); ++i)
    {
        stream << "  " << (values[i] ? "true" : "false") << (i + 1 < values.size() ? ",\n" : "\n");
    }
    stream << "]";

    return static_cast<bool>(stream);
}

// Parses a JSON array of booleans. There is no general-purpose JSON dependency in this project, so
// this only needs to handle the flat `[true, false, ...]` shape the reference fixtures use.
bool read_bool_array(const std::string& path, std::vector<bool>& out)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        std::cerr << "Failed to read results file: " << path << "\n";
        return false;
    }

    std::ostringstream buffer;
    buffer << stream.rdbuf();
    auto text = buffer.str();

    size_t pos = 0;
    while (pos < text.size())
    {
        if (std::isspace(static_cast<unsigned char>(text[pos])) || text[pos] == '[' || text[pos] == ']'
            || text[pos] == ',')
        {
            ++pos;
        }
        else if (text.compare(pos, 4, "true") == 0)
        {
            out.push_back(true);
            pos += 4;
        }
        else if (text.compare(pos, 5, "false") == 0)
        {
            out.push_back(false);
            pos += 5;
        }
        else
        {
            std::cerr << "Unexpected character in results file at offset " << pos << ": '"
                      << text[pos] << "'\n";
            return false;
        }
    }

    return true;
}

} // namespace

int main(int argc, char** argv)
{
    auto usage = [argv]()
    {
        std::cerr << "Usage:\n"
                  << "  " << argv[0] << " <vad-model.aicmodel> <input.wav> <expected.json>\n"
                  << "  " << argv[0] << " --generate <vad-model.aicmodel> <input.wav> <output.json>\n";
    };

    auto generate = argc > 1 && argv[1] != nullptr && std::string(argv[1]) == "--generate";
    auto first    = generate ? 2 : 1;

    if (argc < first + 3)
    {
        usage();
        return 1;
    }
    for (int i = first; i < first + 3; ++i)
    {
        if (argv[i] == nullptr)
        {
            usage();
            return 1;
        }
    }

    auto model_path = std::string(argv[first]);
    auto input_path = std::string(argv[first + 1]);
    // In compare mode this is the reference to match, in generate mode the file to write.
    auto reference_path = std::string(argv[first + 2]);

    std::cout << "SDK version: " << aic::get_sdk_version() << "\n";
    std::cout << "Compatible model version: " << aic::get_compatible_model_version() << "\n";

    auto license_env = std::getenv("AIC_SDK_LICENSE");
    if (!license_env || std::string(license_env).empty())
    {
        std::cerr << "Error: AIC_SDK_LICENSE environment variable not set\n";
        return 1;
    }
    auto license_key = std::string(license_env);

    Audio input;
    if (!load_mono_wav(input_path, input))
    {
        return 1;
    }

    std::cout << "Input: " << input_path << " (" << input.samples.size() << " samples @ "
              << input.sample_rate << " Hz)\n";
    std::cout << (generate ? "Output: " : "Reference: ") << reference_path << "\n";

    auto model_result = aic::Model::create_from_file(model_path);
    if (!model_result.ok())
    {
        std::cerr << "Model creation failed with error: " << static_cast<int>(model_result.error)
                  << "\n";
        return 1;
    }
    auto model = model_result.take();
    std::cout << "Model: " << model.get_id() << "\n";

    // Only dedicated VAD models are accepted; using the file's own sample rate mirrors the
    // reference generator, which never resamples the fixture audio.
    auto block_size = model.get_optimal_block_size(input.sample_rate);

    auto vad_result = aic::Vad::create(model, license_key);
    if (!vad_result.ok())
    {
        std::cerr << "VAD creation failed with error: " << static_cast<int>(vad_result.error)
                  << "\n";
        return 1;
    }
    auto vad = vad_result.take();

    auto err = vad.initialize(input.sample_rate, block_size, false);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "VAD initialization failed with error: " << static_cast<int>(err) << "\n";
        return 1;
    }

    auto context_result = vad.create_context();
    if (!context_result.ok())
    {
        std::cerr << "VAD context creation failed with error: "
                  << static_cast<int>(context_result.error) << "\n";
        return 1;
    }
    auto context = context_result.take();

    // Matches the reference generator's `chunks_exact`: any trailing partial block is dropped so
    // the block count, and therefore the result count, is deterministic.
    std::vector<bool> speech_detected;
    auto              full_blocks = input.samples.size() / block_size;
    for (size_t block = 0; block < full_blocks; ++block)
    {
        auto* chunk = input.samples.data() + block * block_size;

        err = vad.process(chunk, block_size);
        if (err != aic::ErrorCode::Success)
        {
            std::cerr << "VAD processing failed with error: " << static_cast<int>(err) << "\n";
            return 1;
        }

        speech_detected.push_back(context.is_speech_detected());
    }

    if (generate)
    {
        if (!write_bool_array(reference_path, speech_detected))
        {
            return 1;
        }

        std::cout << "Wrote reference output for " << model.get_id() << "\n";
        return 0;
    }

    std::vector<bool> expected;
    if (!read_bool_array(reference_path, expected))
    {
        return 1;
    }

    if (speech_detected != expected)
    {
        if (speech_detected.size() != expected.size())
        {
            std::cerr << "Block count mismatch: actual=" << speech_detected.size()
                      << ", expected=" << expected.size() << "\n";
        }
        else
        {
            for (size_t i = 0; i < speech_detected.size(); ++i)
            {
                if (speech_detected[i] != expected[i])
                {
                    std::cerr << "Mismatch at block " << i
                              << ": actual=" << (speech_detected[i] ? "true" : "false")
                              << ", expected=" << (expected[i] ? "true" : "false") << "\n";
                }
            }
        }

        std::cerr << "End-to-end VAD test FAILED\n";
        return 1;
    }

    std::cout << "All " << speech_detected.size() << " blocks match\n";
    std::cout << "End-to-end VAD test passed\n";
    return 0;
}
