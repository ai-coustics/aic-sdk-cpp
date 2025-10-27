#pragma once

#include <cassert>
#include <memory>
#include <string>
#include <utility>

// Use the new C header name from your snippet.
#include "aic.h"

namespace aic
{

/// Error codes returned by SDK functions
enum class ErrorCode : int
{
    /// Operation completed successfully
    Success = AIC_ERROR_CODE_SUCCESS,
    /// Required pointer argument was NULL. Check all pointer parameters.
    NullPointer = AIC_ERROR_CODE_NULL_POINTER,
    /// Parameter value is outside the acceptable range. Check documentation for valid values.
    ParameterOutOfRange = AIC_ERROR_CODE_PARAMETER_OUT_OF_RANGE,
    /// Model must be initialized before calling this operation. Call `aic_model_initialize` first.
    ModelNotInitialized = AIC_ERROR_CODE_MODEL_NOT_INITIALIZED,
    /// Audio configuration (samplerate, num_channels, num_frames) is not supported by the model.
    AudioConfigUnsupported = AIC_ERROR_CODE_AUDIO_CONFIG_UNSUPPORTED,
    /// Audio buffer configuration differs from the one provided during initialization.
    AudioConfigMismatch = AIC_ERROR_CODE_AUDIO_CONFIG_MISMATCH,
    /// SDK key was not authorized or process failed to report usage. Check if you have internet
    /// connection.
    EnhancementNotAllowed = AIC_ERROR_CODE_ENHANCEMENT_NOT_ALLOWED,
    /// Internal error occurred. Contact support.
    InternalError = AIC_ERROR_CODE_INTERNAL_ERROR,
    /// License key format is invalid or corrupted. Verify the key was copied correctly.
    LicenseFormatInvalid = AIC_ERROR_CODE_LICENSE_FORMAT_INVALID,
    /// License version is not compatible with the SDK version. Update SDK or contact support.
    LicenseVersionUnsupported = AIC_ERROR_CODE_LICENSE_VERSION_UNSUPPORTED,
    /// License key has expired
    LicenseExpired = AIC_ERROR_CODE_LICENSE_EXPIRED,
};

/// Available model types for audio enhancement
enum class ModelType : int
{
    /// Native: 48 kHz, 480 frames, 30ms latency
    Quail_L48 = AIC_MODEL_TYPE_QUAIL_L48,
    /// Native: 16 kHz, 160 frames, 30ms latency
    Quail_L16 = AIC_MODEL_TYPE_QUAIL_L16,
    /// Native: 8 kHz, 80 frames, 30ms latency
    Quail_L8 = AIC_MODEL_TYPE_QUAIL_L8,
    /// Native: 48 kHz, 480 frames, 30ms latency
    Quail_S48 = AIC_MODEL_TYPE_QUAIL_S48,
    /// Native: 16 kHz, 160 frames, 30ms latency
    Quail_S16 = AIC_MODEL_TYPE_QUAIL_S16,
    /// Native: 8 kHz, 80 frames, 30ms latency
    Quail_S8 = AIC_MODEL_TYPE_QUAIL_S8,
    /// Native: 48 kHz, 480 frames, 10ms latency
    Quail_XS = AIC_MODEL_TYPE_QUAIL_XS,
    /// Native: 48 kHz, 480 frames, 10ms latency
    Quail_XXS = AIC_MODEL_TYPE_QUAIL_XXS,
};

/// Configurable parameters for audio enhancement
enum class Parameter : int
{
    /// Bypass keeping processing delay (0.0/1.0): 0.0=disabled, 1.0=enabled
    Bypass = AIC_PARAMETER_BYPASS,
    /// Enhancement intensity (0.0-1.0): 0.0=bypass, 1.0=full enhancement
    EnhancementLevel = AIC_PARAMETER_ENHANCEMENT_LEVEL,
    /// Voice gain multiplier (0.1-4.0): linear amplitude multiplier
    VoiceGain = AIC_PARAMETER_VOICE_GAIN,
    /// Noise gate enable (0.0/1.0): 0.0=disabled, 1.0=enabled
    NoiseGateEnable = AIC_PARAMETER_NOISE_GATE_ENABLE,
};

// ---------------------------
// Internal enum conversions
// ---------------------------

inline constexpr ::AicErrorCode to_c(ErrorCode e)
{
    return static_cast<::AicErrorCode>(static_cast<int>(e));
}
inline constexpr ::AicModelType to_c(ModelType m)
{
    return static_cast<::AicModelType>(static_cast<int>(m));
}
inline constexpr ::AicParameter to_c(Parameter p)
{
    return static_cast<::AicParameter>(static_cast<int>(p));
}

inline constexpr ErrorCode to_cpp(::AicErrorCode e)
{
    return static_cast<ErrorCode>(static_cast<int>(e));
}

// ---------------------------
// C++ wrapper class
// ---------------------------

/**
 * C++ wrapper for the ai-coustics audio enhancement SDK.
 *
 * Provides a modern C++ interface around the C API with RAII resource management.
 * Multiple models can be created to process different audio streams simultaneously
 * or to switch between different enhancement algorithms during runtime.
 *
 * - No exceptions
 * - No convenience functions
 * - Thin RAII around the C API
 */
class AicModel
{
  private:
    std::unique_ptr<::AicModel, void (*)(::AicModel*)> model_;

  public:
    // Constructor for internal use by create() method
    explicit AicModel(::AicModel* model) : model_(model, aic_model_destroy) {}

    /**
     * Creates a new audio enhancement model instance.
     *
     * Multiple models can be created to process different audio streams simultaneously
     * or to switch between different enhancement algorithms during runtime.
     *
     * @param model_type The enhancement algorithm variant to use
     * @param license_key Your license key as a null-terminated string
     * @return Pair containing Model pointer and error code.
     *         If successful, the pointer is valid and error is ErrorCode::Success.
     *         If failed, the pointer is null and error indicates the reason:
     *         - ErrorCode::LicenseInvalid: License key format is incorrect
     *         - ErrorCode::LicenseVersionUnsupported: not compatible with SDK version
     *         - ErrorCode::LicenseExpired: License key has expired
     */
    static std::pair<std::unique_ptr<AicModel>, ErrorCode> create(ModelType          model_type,
                                                                  const std::string& license_key)
    {
        ::AicModel*    raw_model = nullptr;
        ::AicErrorCode rc = aic_model_create(&raw_model, to_c(model_type), license_key.c_str());

        if (rc == AIC_ERROR_CODE_SUCCESS)
        {
            return {std::unique_ptr<AicModel>(new AicModel(raw_model)), ErrorCode::Success};
        }
        else
        {
            return {std::unique_ptr<AicModel>(), to_cpp(rc)};
        }
    }

    // Disable copy constructor and assignment
    AicModel(const AicModel&)            = delete;
    AicModel& operator=(const AicModel&) = delete;

    /**
     * Configures the model for a specific audio format.
     *
     * This function must be called before processing any audio.
     * For the lowest delay, use the sample rate and frame size returned by
     * get_optimal_sample_rate() and get_optimal_num_frames().
     *
     * @warning Do not call from audio processing threads as this allocates memory.
     *
     * @note All channels are mixed to mono for processing. To process channels
     *       independently, create separate model instances.
     *
     * @param sample_rate Audio sample rate in Hz (8000 - 192000)
     * @param num_channels Number of audio channels (1 for mono, 2 for stereo, etc.)
     * @param num_frames Number of samples per channel in each process call
     * @param allow_variable_frames Process can be called with variable number of frames for the
     * cost of higher latency
     * @return ErrorCode::Success if configuration accepted,
     *         ErrorCode::AudioConfigUnsupported if configuration is not supported
     */
    ErrorCode initialize(uint32_t sample_rate, uint16_t num_channels, size_t num_frames,
                         bool allow_variable_frames)
    {
        ::AicErrorCode rc = aic_model_initialize(model_.get(), sample_rate, num_channels,
                                                 num_frames, allow_variable_frames);
        return to_cpp(rc);
    }

    /**
     * Clears all internal state and buffers.
     *
     * Call this when the audio stream is interrupted or when seeking
     * to prevent artifacts from previous audio content.
     * The model stays initialized to the configured settings.
     *
     * @note Real-time safe. Can be called from audio processing threads.
     *
     * Wrapper keeps the previous behavior: void + assert on success.
     */
    void reset()
    {
        ::AicErrorCode rc = aic_model_reset(model_.get());
        (void) rc;
        assert(rc == AIC_ERROR_CODE_SUCCESS);
    }

    /**
     * Processes audio with separate buffers for each channel (planar layout).
     *
     * Enhances speech in the provided audio buffers in-place.
     * The planar function allows a maximum of 16 channels.
     *
     * @param audio Array of channel buffer pointers. Must not be null.
     * @param num_channels Number of channels (must match initialization)
     * @param num_frames Number of samples per channel (must match initialization)
     * @return ErrorCode::Success if audio processed successfully,
     *         ErrorCode::NotInitialized if model has not been initialized,
     *         ErrorCode::AudioConfigMismatch if channel or frame count mismatch
     *         ErrorCode::EnhancementNotAllowed if backend blocks or could not be contacted
     */
    ErrorCode process_planar(float* const* audio, uint16_t num_channels, size_t num_frames)
    {
        ::AicErrorCode rc = aic_model_process_planar(model_.get(), audio, num_channels, num_frames);
        return to_cpp(rc);
    }

    /**
     * Processes audio with interleaved channel data.
     *
     * Enhances speech in the provided audio buffer in-place.
     *
     * @param audio Interleaved audio buffer. Must not be null and exactly of size
     *              num_channels * num_frames.
     * @param num_channels Number of channels (must match initialization)
     * @param num_frames Number of frames (must match initialization)
     * @return ErrorCode::Success if audio processed successfully,
     *         ErrorCode::NotInitialized if model has not been initialized,
     *         ErrorCode::AudioConfigMismatch if channel or frame count mismatch
     *         ErrorCode::EnhancementNotAllowed if backend blocks or could not be contacted
     */
    ErrorCode process_interleaved(float* audio, uint16_t num_channels, size_t num_frames)
    {
        ::AicErrorCode rc =
            aic_model_process_interleaved(model_.get(), audio, num_channels, num_frames);
        return to_cpp(rc);
    }

    /**
     * Modifies a model parameter.
     *
     * All parameters can be changed during audio processing.
     * This function can be called from any thread.
     *
     * Parameter ranges:
     * - EnhancementLevel: 0.0 to 1.0 (0.0=bypass, 1.0=full enhancement)
     * - VoiceGain: 0.1 to 4.0 (linear amplitude multiplier, 1.0=no change)
     * - NoiseGateEnable: 0.0 or 1.0 (0.0=disabled, 1.0=enabled)
     *
     * @param parameter Parameter to modify
     * @param value New parameter value (see ranges above)
     * @return ErrorCode::Success if parameter updated successfully,
     *         ErrorCode::ParameterOutOfRange if value outside valid range
     */
    ErrorCode set_parameter(Parameter parameter, float value)
    {
        ::AicErrorCode rc = aic_model_set_parameter(model_.get(), to_c(parameter), value);
        return to_cpp(rc);
    }

    /**
     * Retrieves the current value of a parameter.
     *
     * This function can be called from any thread.
     * This method keeps the old behavior: assert on success, return value.
     *
     * @param parameter Parameter to query
     * @return Current parameter value
     */
    float get_parameter(Parameter parameter) const
    {
        float          value = 0.0f;
        ::AicErrorCode rc    = aic_model_get_parameter(model_.get(), to_c(parameter), &value);
        assert(rc == AIC_ERROR_CODE_SUCCESS);
        (void) rc;
        return value;
    }

    /**
     * Returns the total output delay in samples for the current audio configuration.
     *
     * This function provides the complete end-to-end latency introduced by the model,
     * which includes both algorithmic processing delay and any buffering overhead.
     * Use this value to synchronize enhanced audio with other streams or to implement
     * delay compensation in your application.
     *
     * Delay behavior:
     * - Before initialization: Returns the base processing delay using the model's
     *   optimal frame size at its native sample rate
     * - After initialization: Returns the actual delay for your specific configuration,
     *   including any additional buffering introduced by non-optimal frame sizes
     *
     * The delay value is always expressed in samples at the sample rate you configured
     * during initialize(). To convert to time units:
     * delay_ms = (delay_samples * 1000) / sample_rate
     *
     * @note Using frame sizes different from the optimal value returned by
     *       get_optimal_num_frames() will increase the delay beyond the model's base latency.
     *
     * Keeps previous behavior (assert on success).
     *
     * @return Processing latency in samples
     */
    size_t get_output_delay() const
    {
        size_t         latency = 0;
        ::AicErrorCode rc      = aic_get_output_delay(model_.get(), &latency);
        assert(rc == AIC_ERROR_CODE_SUCCESS);
        (void) rc;
        return latency;
    }

    /**
     * Retrieves the native sample rate of the selected model.
     *
     * Each model is optimized for a specific sample rate, which determines the frequency
     * range of the enhanced audio output. While you can process audio at any sample rate,
     * understanding the model's native rate helps predict the enhancement quality.
     *
     * How sample rate affects enhancement:
     * - Models trained at lower sample rates (e.g., 8 kHz) can only enhance frequencies
     *   up to their Nyquist limit (4 kHz for 8 kHz models)
     * - When processing higher sample rate input (e.g., 48 kHz) with a lower-rate model,
     *   only the lower frequency components will be enhanced
     *
     * Enhancement blending:
     * When enhancement strength is set below 1.0, the enhanced signal is blended with
     * the original, maintaining the full frequency spectrum of your input while adding
     * the model's noise reduction capabilities to the lower frequencies.
     *
     * @note For maximum enhancement quality across the full frequency spectrum,
     *       match your input sample rate to the model's native rate when possible.
     *
     * Keeps previous behavior (assert on success).
     *
     * @return Optimal sample rate in Hz
     */
    uint32_t get_optimal_sample_rate() const
    {
        uint32_t       sample_rate = 0;
        ::AicErrorCode rc          = aic_get_optimal_sample_rate(model_.get(), &sample_rate);
        assert(rc == AIC_ERROR_CODE_SUCCESS);
        (void) rc;
        return sample_rate;
    }

    /**
     * Retrieves the native number of frames for the selected model and sample rate.
     *
     * Using the optimal number of frames minimizes latency by avoiding internal buffering.
     * When you use a different frame count than the optimal value, the model will
     * introduce additional buffering latency on top of its base processing delay.
     *
     * The optimal frame count adjusts dynamically based on the sample rate used during
     * initialize(). Each time you call initialize() with a different sample rate,
     * the optimal number of frames will update accordingly. Before initialization is called,
     * this function returns the optimal frame count for the model's native sample rate.
     *
     * Each model operates on a fixed time window duration, so the required number of frames
     * varies with sample rate. For example, a model designed for 10 ms processing windows
     * requires 480 frames at 48 kHz, but only 160 frames at 16 kHz to capture the same
     * duration of audio.
     *
     * @param sample_rate The sample rate you want to know the optimal number of frames for
     * @return Optimal number of frames
     */
    size_t get_optimal_num_frames(uint32_t sample_rate) const
    {
        size_t         num_frames = 0;
        ::AicErrorCode rc = aic_get_optimal_num_frames(model_.get(), sample_rate, &num_frames);
        assert(rc == AIC_ERROR_CODE_SUCCESS);
        (void) rc;
        return num_frames;
    }

    /**
     * Returns the version of the SDK.
     *
     * @return A string containing the version (e.g., "1.2.3")
     */
    static std::string get_sdk_version()
    {
        const char* v = ::aic_get_sdk_version();
        return v ? std::string(v) : std::string();
    }
};

} // namespace aic
