#include <catch2/catch_test_macros.hpp>

#include "aic.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

// =====================================================================
// Constants
// =====================================================================

static const char* MODEL_PATH    = "fixtures/sparrow_s_48khz_kg8kp9qm_v52.aicmodel";
static const char* SIGNAL_PATH   = "fixtures/test_signal.wav";
static const char* ENHANCED_PATH = "fixtures/test_signal_enhanced.wav";
static const char* VAD_JSON_PATH = "fixtures/vad_results.json";
static const float EPSILON       = 1e-6f;

// =====================================================================
// Helpers
// =====================================================================

static std::string get_license_key()
{
    const char* env = std::getenv("AIC_SDK_LICENSE");
    REQUIRE(env != nullptr);
    std::string key(env);
    REQUIRE(!key.empty());
    return key;
}

struct TestAudio
{
    uint32_t           sample_rate;
    uint16_t           num_channels;
    size_t             num_frames;
    std::vector<float> interleaved_samples;
};

static uint16_t read_u16_le(const std::vector<uint8_t>& data, size_t offset)
{
    REQUIRE(offset + 1 < data.size());
    return static_cast<uint16_t>(data[offset]) |
           static_cast<uint16_t>(static_cast<uint16_t>(data[offset + 1]) << 8);
}

static uint32_t read_u32_le(const std::vector<uint8_t>& data, size_t offset)
{
    REQUIRE(offset + 3 < data.size());
    return static_cast<uint32_t>(data[offset]) |
           (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
}

static TestAudio load_wav_audio(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    REQUIRE(file.is_open());

    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    REQUIRE(bytes.size() >= 44);

    REQUIRE(std::memcmp(bytes.data(), "RIFF", 4) == 0);
    REQUIRE(std::memcmp(bytes.data() + 8, "WAVE", 4) == 0);

    size_t fmt_offset  = 0;
    size_t fmt_size    = 0;
    size_t data_offset = 0;
    size_t data_size   = 0;

    size_t pos = 12;
    while (pos + 8 <= bytes.size())
    {
        uint32_t chunk_id   = read_u32_le(bytes, pos);
        uint32_t chunk_size = read_u32_le(bytes, pos + 4);
        size_t   payload    = pos + 8;
        REQUIRE(payload + chunk_size <= bytes.size());

        if (chunk_id == 0x20746d66) // "fmt "
        {
            fmt_offset = payload;
            fmt_size   = chunk_size;
        }
        else if (chunk_id == 0x61746164) // "data"
        {
            data_offset = payload;
            data_size   = chunk_size;
        }

        size_t advance = 8 + chunk_size + (chunk_size & 1);
        pos += advance;
    }

    REQUIRE(fmt_offset != 0);
    REQUIRE(data_offset != 0);
    REQUIRE(fmt_size >= 16);

    uint16_t audio_format = read_u16_le(bytes, fmt_offset);
    uint16_t num_channels = read_u16_le(bytes, fmt_offset + 2);
    uint32_t sample_rate  = read_u32_le(bytes, fmt_offset + 4);
    uint16_t bit_depth    = read_u16_le(bytes, fmt_offset + 14);

    REQUIRE(num_channels > 0);
    REQUIRE(bit_depth > 0);
    REQUIRE((bit_depth % 8) == 0);

    bool is_float = (audio_format == 0x0003);
    bool is_pcm   = (audio_format == 0x0001);

    if (audio_format == 0xFFFE && fmt_size >= 40)
    {
        uint32_t subformat_tag = read_u32_le(bytes, fmt_offset + 24);
        if (subformat_tag == 0x00000003)
        {
            is_float = true;
        }
        else if (subformat_tag == 0x00000001)
        {
            is_pcm = true;
        }
    }

    REQUIRE((is_float || is_pcm));

    size_t bytes_per_sample = static_cast<size_t>(bit_depth / 8);
    REQUIRE(bytes_per_sample > 0);
    REQUIRE((data_size % bytes_per_sample) == 0);

    size_t total_samples = data_size / bytes_per_sample;
    REQUIRE((total_samples % num_channels) == 0);

    std::vector<float> interleaved;
    interleaved.reserve(total_samples);

    for (size_t i = 0; i < total_samples; ++i)
    {
        size_t sample_offset = data_offset + i * bytes_per_sample;
        float  sample        = 0.0f;

        if (is_float)
        {
            REQUIRE(bit_depth == 32);
            uint32_t raw = read_u32_le(bytes, sample_offset);
            std::memcpy(&sample, &raw, sizeof(float));
        }
        else
        {
            if (bit_depth == 8)
            {
                int32_t s = static_cast<int32_t>(bytes[sample_offset]) - 128;
                sample = static_cast<float>(static_cast<double>(s) / 128.0);
            }
            else if (bit_depth == 16)
            {
                int16_t s = static_cast<int16_t>(read_u16_le(bytes, sample_offset));
                sample = static_cast<float>(static_cast<double>(s) / 32768.0);
            }
            else if (bit_depth == 24)
            {
                int32_t s = static_cast<int32_t>(bytes[sample_offset]) |
                            (static_cast<int32_t>(bytes[sample_offset + 1]) << 8) |
                            (static_cast<int32_t>(bytes[sample_offset + 2]) << 16);
                if ((s & 0x00800000) != 0)
                {
                    s |= 0xFF000000;
                }
                sample = static_cast<float>(static_cast<double>(s) / 8388608.0);
            }
            else if (bit_depth == 32)
            {
                int32_t s = static_cast<int32_t>(read_u32_le(bytes, sample_offset));
                sample = static_cast<float>(static_cast<double>(s) / 2147483648.0);
            }
            else
            {
                FAIL("Unsupported PCM bit depth");
            }
        }

        interleaved.push_back(sample);
    }

    return {sample_rate, num_channels, interleaved.size() / num_channels, std::move(interleaved)};
}

static std::vector<float> interleaved_to_sequential(const std::vector<float>& interleaved,
                                                     uint16_t                  num_channels,
                                                     size_t                    num_frames)
{
    std::vector<float> seq(interleaved.size());
    for (size_t frame = 0; frame < num_frames; ++frame)
    {
        for (uint16_t ch = 0; ch < num_channels; ++ch)
        {
            seq[static_cast<size_t>(ch) * num_frames + frame] =
                interleaved[frame * num_channels + ch];
        }
    }
    return seq;
}

static std::vector<float> sequential_to_interleaved(const std::vector<float>& sequential,
                                                     uint16_t                  num_channels,
                                                     size_t                    num_frames)
{
    std::vector<float> interleaved(sequential.size());
    for (size_t frame = 0; frame < num_frames; ++frame)
    {
        for (uint16_t ch = 0; ch < num_channels; ++ch)
        {
            interleaved[frame * num_channels + ch] =
                sequential[static_cast<size_t>(ch) * num_frames + frame];
        }
    }
    return interleaved;
}

static std::vector<std::vector<float>> interleaved_to_planar(const std::vector<float>& interleaved,
                                                              uint16_t num_channels,
                                                              size_t   num_frames)
{
    std::vector<std::vector<float>> planar(num_channels, std::vector<float>(num_frames));
    for (size_t frame = 0; frame < num_frames; ++frame)
    {
        for (uint16_t ch = 0; ch < num_channels; ++ch)
        {
            planar[ch][frame] = interleaved[frame * num_channels + ch];
        }
    }
    return planar;
}

static std::vector<float> planar_to_interleaved(const std::vector<std::vector<float>>& planar,
                                                 uint16_t num_channels,
                                                 size_t   num_frames)
{
    std::vector<float> interleaved(static_cast<size_t>(num_channels) * num_frames);
    for (size_t frame = 0; frame < num_frames; ++frame)
    {
        for (uint16_t ch = 0; ch < num_channels; ++ch)
        {
            interleaved[frame * num_channels + ch] = planar[ch][frame];
        }
    }
    return interleaved;
}

static void compare_samples(const std::vector<float>& actual,
                             const std::vector<float>& expected,
                             float                     epsilon)
{
    REQUIRE(actual.size() == expected.size());
    for (size_t i = 0; i < actual.size(); ++i)
    {
        INFO("Sample index " << i << ": actual=" << actual[i] << " expected=" << expected[i]);
        REQUIRE(std::fabs(actual[i] - expected[i]) < epsilon);
    }
}

static std::vector<bool> parse_bool_array_json(const std::string& path)
{
    std::ifstream file(path);
    REQUIRE(file.is_open());

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    std::vector<bool> result;
    size_t            pos = 0;
    while (pos < content.size())
    {
        if (content.compare(pos, 4, "true") == 0)
        {
            result.push_back(true);
            pos += 4;
        }
        else if (content.compare(pos, 5, "false") == 0)
        {
            result.push_back(false);
            pos += 5;
        }
        else
        {
            ++pos;
        }
    }

    REQUIRE(!result.empty());
    return result;
}

// =====================================================================
// Test setup
// =====================================================================

struct TestSetup
{
    aic::Processor        processor;
    aic::ProcessorContext  ctx;
    aic::VadContext        vad;
    uint32_t              sample_rate;
    uint16_t              num_channels;
    size_t                num_frames;

    static TestSetup create(uint32_t sample_rate, uint16_t num_channels, size_t num_frames)
    {
        auto model_result = aic::Model::create_from_file(MODEL_PATH);
        REQUIRE(model_result.ok());
        auto model = model_result.take();

        auto proc_result = aic::Processor::create(model, get_license_key());
        REQUIRE(proc_result.ok());
        auto processor = proc_result.take();

        auto err = processor.initialize(sample_rate, num_channels, num_frames, false);
        REQUIRE(err == aic::ErrorCode::Success);

        auto ctx_result = processor.create_context();
        REQUIRE(ctx_result.ok());
        auto ctx = ctx_result.take();

        auto vad_result = processor.create_vad_context();
        REQUIRE(vad_result.ok());
        auto vad = vad_result.take();

        return {std::move(processor), std::move(ctx), std::move(vad),
                sample_rate, num_channels, num_frames};
    }

    static TestSetup create_with_optimal_frames(uint32_t sample_rate, uint16_t num_channels)
    {
        auto model_result = aic::Model::create_from_file(MODEL_PATH);
        REQUIRE(model_result.ok());
        auto model = model_result.take();

        size_t num_frames = model.get_optimal_num_frames(sample_rate);

        auto proc_result = aic::Processor::create(model, get_license_key());
        REQUIRE(proc_result.ok());
        auto processor = proc_result.take();

        auto err = processor.initialize(sample_rate, num_channels, num_frames, false);
        REQUIRE(err == aic::ErrorCode::Success);

        auto ctx_result = processor.create_context();
        REQUIRE(ctx_result.ok());
        auto ctx = ctx_result.take();

        auto vad_result = processor.create_vad_context();
        REQUIRE(vad_result.ok());
        auto vad = vad_result.take();

        return {std::move(processor), std::move(ctx), std::move(vad),
                sample_rate, num_channels, num_frames};
    }
};

// =====================================================================
// Tests
// =====================================================================

TEST_CASE("process_full_file", "[processing]")
{
    auto input = load_wav_audio(SIGNAL_PATH);

    auto setup = TestSetup::create(input.sample_rate, input.num_channels, input.num_frames);

    auto err = setup.ctx.set_parameter(aic::ProcessorParameter::EnhancementLevel, 0.9f);
    REQUIRE(err == aic::ErrorCode::Success);

    auto samples = input.interleaved_samples;
    err = setup.processor.process_interleaved(samples.data(), input.num_channels, input.num_frames);
    REQUIRE(err == aic::ErrorCode::Success);

    auto expected = load_wav_audio(ENHANCED_PATH).interleaved_samples;

    compare_samples(samples, expected, EPSILON);
}

TEST_CASE("process_full_file_sequential", "[processing]")
{
    auto input = load_wav_audio(SIGNAL_PATH);

    auto setup = TestSetup::create(input.sample_rate, input.num_channels, input.num_frames);

    auto err = setup.ctx.set_parameter(aic::ProcessorParameter::EnhancementLevel, 0.9f);
    REQUIRE(err == aic::ErrorCode::Success);

    auto sequential =
        interleaved_to_sequential(input.interleaved_samples, input.num_channels, input.num_frames);

    err = setup.processor.process_sequential(sequential.data(), input.num_channels, input.num_frames);
    REQUIRE(err == aic::ErrorCode::Success);

    auto result =
        sequential_to_interleaved(sequential, input.num_channels, input.num_frames);

    auto expected = load_wav_audio(ENHANCED_PATH).interleaved_samples;

    compare_samples(result, expected, EPSILON);
}

TEST_CASE("process_full_file_planar", "[processing]")
{
    auto input = load_wav_audio(SIGNAL_PATH);

    auto setup = TestSetup::create(input.sample_rate, input.num_channels, input.num_frames);

    auto err = setup.ctx.set_parameter(aic::ProcessorParameter::EnhancementLevel, 0.9f);
    REQUIRE(err == aic::ErrorCode::Success);

    auto planar =
        interleaved_to_planar(input.interleaved_samples, input.num_channels, input.num_frames);

    std::vector<float*> channel_ptrs(input.num_channels);
    for (uint16_t ch = 0; ch < input.num_channels; ++ch)
    {
        channel_ptrs[ch] = planar[ch].data();
    }

    err = setup.processor.process_planar(channel_ptrs.data(), input.num_channels, input.num_frames);
    REQUIRE(err == aic::ErrorCode::Success);

    auto result = planar_to_interleaved(planar, input.num_channels, input.num_frames);

    auto expected = load_wav_audio(ENHANCED_PATH).interleaved_samples;

    compare_samples(result, expected, EPSILON);
}

TEST_CASE("process_blocks_with_vad", "[vad]")
{
    auto input = load_wav_audio(SIGNAL_PATH);

    auto setup = TestSetup::create_with_optimal_frames(input.sample_rate, input.num_channels);

    auto err = setup.ctx.set_parameter(aic::ProcessorParameter::Bypass, 1.0f);
    REQUIRE(err == aic::ErrorCode::Success);

    auto audio      = input.interleaved_samples;
    auto block_size = setup.num_frames;
    auto total_frames = input.num_frames;

    std::vector<bool> actual_vad;
    for (size_t offset = 0; offset + block_size <= total_frames; offset += block_size)
    {
        float* block_ptr = audio.data() + (offset * input.num_channels);
        err = setup.processor.process_interleaved(block_ptr, input.num_channels, block_size);
        REQUIRE(err == aic::ErrorCode::Success);
        actual_vad.push_back(setup.vad.is_speech_detected());
    }

    auto expected_vad = parse_bool_array_json(VAD_JSON_PATH);

    REQUIRE(actual_vad.size() == expected_vad.size());
    for (size_t i = 0; i < actual_vad.size(); ++i)
    {
        INFO("VAD block " << i << ": actual=" << actual_vad[i]
                          << " expected=" << expected_vad[i]);
        REQUIRE(actual_vad[i] == expected_vad[i]);
    }
}

TEST_CASE("process_blocks_with_vad_and_enhancement", "[vad]")
{
    auto input = load_wav_audio(SIGNAL_PATH);

    auto setup = TestSetup::create_with_optimal_frames(input.sample_rate, input.num_channels);

    auto err = setup.ctx.set_parameter(aic::ProcessorParameter::EnhancementLevel, 0.5f);
    REQUIRE(err == aic::ErrorCode::Success);

    auto audio      = input.interleaved_samples;
    auto block_size = setup.num_frames;
    auto total_frames = input.num_frames;

    std::vector<bool> actual_vad;
    for (size_t offset = 0; offset + block_size <= total_frames; offset += block_size)
    {
        float* block_ptr = audio.data() + (offset * input.num_channels);
        err = setup.processor.process_interleaved(block_ptr, input.num_channels, block_size);
        REQUIRE(err == aic::ErrorCode::Success);
        actual_vad.push_back(setup.vad.is_speech_detected());
    }

    auto expected_vad = parse_bool_array_json(VAD_JSON_PATH);

    REQUIRE(actual_vad.size() == expected_vad.size());
    for (size_t i = 0; i < actual_vad.size(); ++i)
    {
        INFO("VAD block " << i << ": actual=" << actual_vad[i]
                          << " expected=" << expected_vad[i]);
        REQUIRE(actual_vad[i] == expected_vad[i]);
    }
}
