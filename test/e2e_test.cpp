// End-to-end enhancement test.
//
// Loads an audio file, enhances it with a Processor, and compares the result sample by sample
// against a pre-generated reference file. This mirrors the reference test in the C SDK
// (aic-sdk-interface's `process_full_file`), so the same fixtures and the same tolerance apply.
//
// The whole file is passed to `Processor::process` in a single call, which is deliberately not the
// model's optimal block size: it exercises the internal block adapter with an arbitrary input
// length. The enhancement level is set to 0.9 to cover a non-default parameter path.
//
// Usage:
//   aic-sdk-e2e-test <model.aicmodel> <input.wav> <expected.wav> [epsilon]
//   aic-sdk-e2e-test --generate <model.aicmodel> <input.wav> <output.wav>
//
// The second form regenerates a reference file instead of comparing against one. Use it when a
// model is added or when a deliberate change to the SDK moves the expected output.
//
// Fixtures are 32-bit float mono WAV so that loading them involves no integer scaling and the
// samples reaching the model are exactly the ones the reference was generated from.

#include "AudioFile.h"
#include "aic.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace
{

struct Audio
{
    uint32_t           sample_rate;
    std::vector<float> samples;
};

uint32_t read_little_endian_u32(const unsigned char* bytes)
{
    return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8)
           | (static_cast<uint32_t>(bytes[2]) << 16)
           | (static_cast<uint32_t>(bytes[3]) << 24);
}

// AudioFile 1.1.4 accepts WAVE_FORMAT_EXTENSIBLE but ignores its sub-format. In particular, it
// silently interprets 32-bit float samples as signed PCM. Detect the container before handing it to
// AudioFile so a future fixture replacement fails rather than producing plausible but wrong audio.
bool uses_extensible_wave_format(const std::string& path)
{
    std::ifstream stream(path, std::ios::binary);
    unsigned char header[12] = {};
    if (!stream.read(reinterpret_cast<char*>(header), sizeof(header))
        || std::memcmp(header, "RIFF", 4) != 0 || std::memcmp(header + 8, "WAVE", 4) != 0)
    {
        return false; // Let AudioFile report missing, truncated, and non-WAV inputs.
    }

    unsigned char chunk_header[8] = {};
    while (stream.read(reinterpret_cast<char*>(chunk_header), sizeof(chunk_header)))
    {
        auto chunk_size = read_little_endian_u32(chunk_header + 4);
        if (std::memcmp(chunk_header, "fmt ", 4) == 0)
        {
            unsigned char format[2] = {};
            return chunk_size >= sizeof(format)
                   && stream.read(reinterpret_cast<char*>(format), sizeof(format))
                   && format[0] == 0xfe && format[1] == 0xff;
        }

        // RIFF chunks are padded to an even byte boundary; the padding is not part of chunk_size.
        stream.seekg(static_cast<std::streamoff>(chunk_size) + (chunk_size & 1u), std::ios::cur);
    }

    return false;
}

// Loads the first channel of a WAV file. Returns false and reports the reason on failure.
bool load_mono_wav(const std::string& path, Audio& out)
{
    if (uses_extensible_wave_format(path))
    {
        std::cerr << "Unsupported WAVE_FORMAT_EXTENSIBLE file: " << path << "\n"
                  << "AudioFile 1.1.4 can silently misdecode 32-bit extensible float samples; "
                     "convert the file to WAVE_FORMAT_IEEE_FLOAT (format tag 3).\n";
        return false;
    }

    AudioFile<float> file;

    // AudioFile prints its own diagnostics on stdout; keep them, they explain load failures.
    if (!file.load(path))
    {
        std::cerr << "Failed to load audio file: " << path << "\n";
        return false;
    }

    if (file.getNumChannels() < 1 || file.getNumSamplesPerChannel() < 1)
    {
        std::cerr << "Audio file has no samples: " << path << "\n";
        return false;
    }

    out.sample_rate = static_cast<uint32_t>(file.getSampleRate());
    out.samples     = file.samples[0]; // mono: first channel only
    return true;
}

// Writes mono audio as a 32-bit float WAV, the format the fixtures use.
bool write_mono_wav(const std::string& path, const std::vector<float>& samples,
                    uint32_t sample_rate)
{
    AudioFile<float> file;
    file.setAudioBuffer({samples});
    file.setSampleRate(static_cast<int>(sample_rate));
    file.setBitDepth(32);

    if (!file.save(path, AudioFileFormat::Wave))
    {
        std::cerr << "Failed to write audio file: " << path << "\n";
        return false;
    }

    return true;
}

// Compares the processed audio against the reference. Reports the first offending sample and the
// largest deviation seen, so a failure says how far off the run was, not just that it differed.
bool compare_samples(const std::vector<float>& actual, const std::vector<float>& expected,
                     float epsilon)
{
    if (actual.size() != expected.size())
    {
        std::cerr << "Sample count mismatch: actual=" << actual.size()
                  << ", expected=" << expected.size() << "\n";
        return false;
    }

    size_t first_mismatch = actual.size();
    size_t mismatches     = 0;
    float  max_diff       = 0.0f;
    size_t max_diff_index = 0;

    for (size_t i = 0; i < actual.size(); ++i)
    {
        float diff = std::fabs(actual[i] - expected[i]);

        if (diff > max_diff)
        {
            max_diff       = diff;
            max_diff_index = i;
        }

        if (diff > epsilon)
        {
            if (mismatches == 0)
            {
                first_mismatch = i;
            }
            ++mismatches;
        }
    }

    std::cerr << std::scientific << std::setprecision(6);

    if (mismatches > 0)
    {
        std::cerr << "Sample mismatch at index " << first_mismatch
                  << ": actual=" << actual[first_mismatch]
                  << ", expected=" << expected[first_mismatch]
                  << ", diff=" << std::fabs(actual[first_mismatch] - expected[first_mismatch])
                  << "\n";
        std::cerr << mismatches << " of " << actual.size() << " samples exceed epsilon " << epsilon
                  << "\n";
        std::cerr << "Largest deviation: " << max_diff << " at index " << max_diff_index << "\n";
        return false;
    }

    std::cout << "All " << actual.size() << " samples match within epsilon " << epsilon
              << " (largest deviation " << max_diff << ")\n";
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    auto usage = [argv]()
    {
        std::cerr << "Usage:\n"
                  << "  " << argv[0] << " <model.aicmodel> <input.wav> <expected.wav> [epsilon]\n"
                  << "  " << argv[0] << " --generate <model.aicmodel> <input.wav> <output.wav>\n";
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

    // The C SDK's reference test compares with an absolute tolerance of 1e-6.
    auto epsilon = 1e-6f;
    if (!generate && argc > 4 && argv[4] != nullptr)
    {
        epsilon = static_cast<float>(std::atof(argv[4]));
    }

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

    Audio reference;
    if (!generate)
    {
        if (!load_mono_wav(reference_path, reference))
        {
            return 1;
        }

        if (input.sample_rate != reference.sample_rate)
        {
            std::cerr << "Sample rate mismatch between input (" << input.sample_rate
                      << " Hz) and reference (" << reference.sample_rate << " Hz)\n";
            return 1;
        }
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

    auto processor_result = aic::Processor::create(model, license_key);
    if (!processor_result.ok())
    {
        std::cerr << "Processor creation failed with error: "
                  << static_cast<int>(processor_result.error) << "\n";
        return 1;
    }
    auto processor = processor_result.take();

    // One block holding the whole file: a deliberately non-optimal block size.
    auto err = processor.initialize(input.sample_rate, input.samples.size(), false);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Processor initialization failed with error: " << static_cast<int>(err)
                  << "\n";
        return 1;
    }

    auto context_result = processor.create_context();
    if (!context_result.ok())
    {
        std::cerr << "Processor context creation failed with error: "
                  << static_cast<int>(context_result.error) << "\n";
        return 1;
    }
    auto context = context_result.take();

    err = context.set_parameter(aic::ProcessorParameter::EnhancementLevel, 0.9f);
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Failed to set enhancement level: " << static_cast<int>(err) << "\n";
        return 1;
    }

    // Enhancement happens in-place.
    auto enhanced = input.samples;

    err = processor.process(enhanced.data(), enhanced.size());
    if (err != aic::ErrorCode::Success)
    {
        std::cerr << "Processing failed with error: " << static_cast<int>(err) << "\n";
        return 1;
    }

    if (generate)
    {
        if (!write_mono_wav(reference_path, enhanced, input.sample_rate))
        {
            return 1;
        }

        std::cout << "Wrote reference output for " << model.get_id() << "\n";
        return 0;
    }

    if (!compare_samples(enhanced, reference.samples, epsilon))
    {
        std::cerr << "End-to-end enhancement test FAILED\n";
        return 1;
    }

    std::cout << "End-to-end enhancement test passed\n";
    return 0;
}
