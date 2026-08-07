// Shared WAV loading/writing helpers for the end-to-end test executables.
//
// Fixtures are 32-bit float mono WAV so that loading them involves no integer scaling and the
// samples reaching the model are exactly the ones the reference was generated from.

#pragma once

#include "AudioFile.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace aic_test
{

struct Audio
{
    uint32_t           sample_rate;
    std::vector<float> samples;
};

inline uint32_t read_little_endian_u32(const unsigned char* bytes)
{
    return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8)
           | (static_cast<uint32_t>(bytes[2]) << 16)
           | (static_cast<uint32_t>(bytes[3]) << 24);
}

// AudioFile 1.1.4 accepts WAVE_FORMAT_EXTENSIBLE but ignores its sub-format. In particular, it
// silently interprets 32-bit float samples as signed PCM. Detect the container before handing it to
// AudioFile so a future fixture replacement fails rather than producing plausible but wrong audio.
inline bool uses_extensible_wave_format(const std::string& path)
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
inline bool load_mono_wav(const std::string& path, Audio& out)
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
inline bool write_mono_wav(const std::string& path, const std::vector<float>& samples,
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

} // namespace aic_test
