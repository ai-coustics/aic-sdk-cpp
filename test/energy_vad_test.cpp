// Integration coverage for energy VAD ownership, shared controls and processor-driven decisions.
#include "aic.hpp"
#include "wav_io.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <vector>

static_assert(!std::is_copy_constructible<aic::EnergyVadContext>::value, "Unique ownership");
static_assert(!std::is_copy_assignable<aic::EnergyVadContext>::value, "Unique ownership");
static_assert(std::is_nothrow_move_constructible<aic::EnergyVadContext>::value, "Movable handle");
static_assert(std::is_nothrow_move_assignable<aic::EnergyVadContext>::value, "Movable handle");

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void check(aic::ErrorCode code)
{
    if (code != aic::ErrorCode::Success)
        throw std::runtime_error("SDK error: " + std::to_string(static_cast<int>(code)));
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        require(argc == 3, "Usage: energy-vad-test <enhancement-model> <input.wav>");
        const char* license = std::getenv("AIC_SDK_LICENSE");
        require(license && *license, "Set AIC_SDK_LICENSE to run this test");
        auto model_result = aic::Model::create_from_file(argv[1]);
        check(model_result.error);
        auto            model = model_result.take();
        aic_test::Audio input;
        require(aic_test::load_mono_wav(argv[2], input), "Could not load audio fixture");

        // Return a live context after the processor and all other handles have been destroyed.
        auto survivor = [&]()
        {
            auto result = aic::Processor::create(model, license);
            check(result.error);
            auto processor      = result.take();
            auto control_result = processor.create_context();
            check(control_result.error);
            auto control      = control_result.take();
            auto first_result = processor.create_energy_vad_context();
            check(first_result.error);
            auto first         = first_result.take();
            auto second_result = processor.create_energy_vad_context();
            check(second_result.error);
            auto second = second_result.take();
            require(!first.is_speech_detected(), "Initial prediction must be false");
            require(first.get_prediction_delay() == control.get_audio_delay(),
                    "Base delay mismatch");

            const auto block_size = model.get_optimal_block_size(input.sample_rate);
            check(processor.initialize(input.sample_rate, block_size, false));
            require(first.get_prediction_delay() == control.get_audio_delay(), "Delay mismatch");
            check(first.set_parameter(aic::VadParameter::Sensitivity, 8.0f));
            require(second.get_parameter(aic::VadParameter::Sensitivity) == 8.0f,
                    "Contexts must share parameters");
            for (float invalid : {0.0f, 16.0f})
                require(first.set_parameter(aic::VadParameter::Sensitivity, invalid) ==
                            aic::ErrorCode::ParameterOutOfRange,
                        "Sensitivity range not enforced");
            check(second.set_parameter(aic::VadParameter::SpeechHoldDuration, 0.0f));
            check(second.set_parameter(aic::VadParameter::MinimumSpeechDuration, 0.0f));
            require(first.get_parameter(aic::VadParameter::SpeechHoldDuration) == 0.0f &&
                        first.get_parameter(aic::VadParameter::MinimumSpeechDuration) == 0.0f,
                    "Duration parameters must be shared");

            // Exercise inference with normal enhancement, bypass and zero enhancement level.
            std::vector<float> block(block_size);
            for (int mode = 0; mode < 3; ++mode)
            {
                check(control.reset());
                require(!first.is_speech_detected(), "Processor reset must clear VAD");
                check(control.set_parameter(aic::ProcessorParameter::Bypass,
                                            mode == 1 ? 1.0f : 0.0f));
                check(control.set_parameter(aic::ProcessorParameter::EnhancementLevel,
                                            mode == 2 ? 0.0f : 1.0f));
                bool detected = false;
                for (size_t offset = 0; offset < input.samples.size(); offset += block_size)
                {
                    std::fill(block.begin(), block.end(), 0.0f);
                    const size_t count = std::min(block_size, input.samples.size() - offset);
                    std::copy_n(input.samples.data() + offset, count, block.data());
                    check(processor.process(block.data(), block.size()));
                    const bool speech = first.is_speech_detected();
                    require(speech == second.is_speech_detected(),
                            "Contexts must share predictions");
                    detected = detected || speech;
                }
                require(detected, "Fixture must trigger speech detection in every output mode");
                check(first.reset());
                require(!second.is_speech_detected(), "Energy VAD reset must clear shared state");
                require(first.get_parameter(aic::VadParameter::Sensitivity) == 8.0f,
                        "Reset must retain parameters");
            }
            check(processor.initialize(input.sample_rate, block_size + 17, true));
            require(first.get_prediction_delay() == control.get_audio_delay(),
                    "Variable block delay mismatch");
            second     = std::move(first);
            auto moved = std::move(second);
            require(moved.get_parameter(aic::VadParameter::Sensitivity) == 8.0f,
                    "Move must preserve the handle");
            return moved;
        }();
        check(survivor.reset());
        require(!survivor.is_speech_detected(),
                "Context must remain valid after processor destruction");
        check(survivor.set_parameter(aic::VadParameter::Sensitivity, 7.0f));
        require(survivor.get_parameter(aic::VadParameter::Sensitivity) == 7.0f,
                "Surviving context controls must remain valid");
        std::cout << "Energy VAD integration checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
