#pragma once

#include "aic.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace aic
{

/**
 * Error codes returned by SDK functions.
 */
enum class ErrorCode : int
{
    /// Operation completed successfully
    Success = AIC_ERROR_CODE_SUCCESS,
    /// Required pointer argument was NULL. Check all pointer parameters.
    NullPointer = AIC_ERROR_CODE_NULL_POINTER,
    /// Parameter value is outside the acceptable range. Check documentation for valid values.
    ParameterOutOfRange = AIC_ERROR_CODE_PARAMETER_OUT_OF_RANGE,
    /// Processor must be initialized before calling this operation. Call Processor::initialize
    /// first.
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
    /// The requested parameter is read-only for this model type and cannot be modified.
    ParameterFixed = AIC_ERROR_CODE_PARAMETER_FIXED,
    /// License key format is invalid or corrupted. Verify the key was copied correctly.
    LicenseFormatInvalid = AIC_ERROR_CODE_LICENSE_FORMAT_INVALID,
    /// License version is not compatible with the SDK version. Update SDK or contact support.
    LicenseVersionUnsupported = AIC_ERROR_CODE_LICENSE_VERSION_UNSUPPORTED,
    /// License key has expired. Renew your license to continue.
    LicenseExpired = AIC_ERROR_CODE_LICENSE_EXPIRED,
    /// The model file is invalid or corrupted. Verify the file is correct.
    ModelInvalid = AIC_ERROR_CODE_MODEL_INVALID,
    /// The model file version is not compatible with this SDK version.
    ModelVersionUnsupported = AIC_ERROR_CODE_MODEL_VERSION_UNSUPPORTED,
    /// The path to the model file is invalid.
    ModelFilePathInvalid = AIC_ERROR_CODE_MODEL_FILE_PATH_INVALID,
    /// The model file cannot be opened due to a filesystem error. Verify that the file exists.
    FileSystemError = AIC_ERROR_CODE_FILE_SYSTEM_ERROR,
    /// The model data is not aligned to 64 bytes.
    ModelDataUnaligned = AIC_ERROR_CODE_MODEL_DATA_UNALIGNED,
};

/**
 * Result wrapper for returning a value and an ErrorCode without exceptions.
 *
 * @tparam T Value type stored in the result.
 */
template <typename T> struct Result
{
    /// The returned value (may be default-constructed on failure).
    T value;
    /// Error status for the operation.
    ErrorCode error;

    // Default constructor: default-constructs the value and sets a non-success error as a safe
    // sentinel
    Result() : value(), error(ErrorCode::InternalError) {}
    // Constructor: copies the value and stores the error code
    Result(const T& v, ErrorCode e) : value(v), error(e) {}
    // Constructor: moves the value and stores the error code
    Result(T&& v, ErrorCode e) : value(std::move(v)), error(e) {}

    /// Returns true when error == ErrorCode::Success.
    bool ok() const
    {
        return error == ErrorCode::Success;
    }
    /// Returns the contained value by move (useful for move-only types).
    T take()
    {
        return std::move(value);
    }
};

/**
 * Configurable parameters for audio processing.
 */
enum class ProcessorParameter : int
{
    /**
     * Controls whether audio processing is bypassed while preserving algorithmic delay.
     *
     * When enabled, the input audio passes through unmodified, but the output is still
     * delayed by the same amount as during normal processing. This ensures seamless
     * transitions when toggling enhancement on/off without audible clicks or timing shifts.
     *
     * **Range:** 0.0 to 1.0
     * - **0.0:** Enhancement active (normal processing)
     * - **1.0:** Bypass enabled (latency-compensated passthrough)
     *
     * **Default:** 0.0
     */
    Bypass = AIC_PROCESSOR_PARAMETER_BYPASS,
    /**
     * Controls the intensity of speech enhancement processing.
     *
     * **Range:** 0.0 to 1.0
     * - **0.0:** No enhancement - original signal passes through unchanged
     * - **1.0:** Full enhancement - maximum noise reduction but also more audible artifacts
     *
     * **Default:** 1.0
     */
    EnhancementLevel = AIC_PROCESSOR_PARAMETER_ENHANCEMENT_LEVEL,
    /**
     * Compensates for perceived volume reduction after noise removal.
     *
     * **Range:** 0.1 to 4.0 (linear amplitude multiplier)
     * - **0.1:** Significant volume reduction (-20 dB)
     * - **1.0:** No gain change (0 dB, default)
     * - **2.0:** Double amplitude (+6 dB)
     * - **4.0:** Maximum boost (+12 dB)
     *
     * **Formula:** Gain (dB) = 20 × log₁₀(value)
     * **Default:** 1.0
     */
    VoiceGain = AIC_PROCESSOR_PARAMETER_VOICE_GAIN,
};

/**
 * Configurable parameters for Voice Activity Detection.
 */
enum class VadParameter : int
{
    /**
     * Controls for how long the VAD continues to detect speech after the audio signal
     * no longer contains speech.
     *
     * The VAD reports speech detected if the audio signal contained speech in at least 50%
     * of the frames processed in the last `speech_hold_duration` seconds.
     *
     * This affects the stability of speech detected -> not detected transitions.
     *
     * NOTE: The VAD returns a value per processed buffer, so this duration is rounded
     * to the closest model window length. For example, if the model has a processing window
     * length of 10 ms, the VAD will round up/down to the closest multiple of 10 ms.
     * Because of this, this parameter may return a different value than the one it was last set to.
     *
     * **Range:** 0.0 to 20x model window length (value in seconds)
     *
     * **Default:** 0.05 (50 ms)
     */
    SpeechHoldDuration = AIC_VAD_PARAMETER_SPEECH_HOLD_DURATION,
    /**
     * Controls the sensitivity (energy threshold) of the VAD.
     *
     * This value is used by the VAD as the threshold a
     * speech audio signal's energy has to exceed in order to be
     * considered speech.
     *
     * **Range:** 1.0 to 15.0
     *
     * **Formula:** Energy threshold = 10 ^ (-sensitivity)
     *
     * **Default:** 6.0
     */
    Sensitivity = AIC_VAD_PARAMETER_SENSITIVITY,
    /**
     * Controls for how long speech needs to be present in the audio signal before
     * the VAD considers it speech.
     *
     * This affects the stability of speech not detected -> detected transitions.
     *
     * NOTE: The VAD returns a value per processed buffer, so this duration is rounded
     * to the closest model window length. For example, if the model has a processing window
     * length of 10 ms, the VAD will round up/down to the closest multiple of 10 ms.
     * Because of this, this parameter may return a different value than the one it was last set to.
     *
     * **Range:** 0.0 to 1.0 (value in seconds)
     *
     * **Default:** 0.0
     */
    MinimumSpeechDuration = AIC_VAD_PARAMETER_MINIMUM_SPEECH_DURATION,
};

// ---------------------------
// Model wrapper
// ---------------------------

class Model
{
  private:
    ::AicModel* model_;

  public:
    // Destructor: releases the underlying SDK model handle if one is owned
    ~Model()
    {
        if (model_)
        {
            aic_model_destroy(model_);
        }
    }

    // Move constructor: the handle from the source Model gets moved into the new Model
    Model(Model&& other) noexcept : model_(other.model_)
    {
        other.model_ = nullptr;
    }

    // Move assignment: replaces the currently owned handle with the source handle and clears the
    // source
    Model& operator=(Model&& other) noexcept
    {
        if (this != &other)
        {
            if (model_)
            {
                aic_model_destroy(model_);
            }
            model_       = other.model_;
            other.model_ = nullptr;
        }
        return *this;
    }

    // Deleted copy constructor: copying is disabled because this wrapper has unique ownership of
    // the handle
    Model(const Model&) = delete;

    // Deleted copy assignment: copying is disabled for the same reason as the copy constructor
    Model& operator=(const Model&) = delete;

    /**
     * Creates a new model instance from a model file.
     *
     * A single model instance can be used to create multiple processors.
     *
     * @param file_path Path to the model file.
     * @return Result containing the Model and an ErrorCode.
     *
     * @note Processor instances retain a shared reference to the model data.
     *       It is safe to destroy the Model after creating the desired processors.
     *       The memory used by the model is freed after all processors are destroyed.
     * @warning Not thread-safe. Ensure no other threads are using the model handle or the same file
     * path.
     */
    static Result<Model> create_from_file(const std::string& file_path);

    /**
     * Creates a new model instance from a memory buffer.
     *
     * The buffer must remain valid and unchanged for the lifetime of the model.
     *
     * @param buffer Pointer to model bytes (must be 64-byte aligned).
     * @param buffer_len Size of the model buffer in bytes.
     * @return Result containing the Model and an ErrorCode.
     *
     * @note Processor instances retain a shared reference to the model data.
     *       It is safe to destroy the Model after creating the desired processors.
     *       The memory used by the model is freed after all processors are destroyed.
     * @warning Not thread-safe. Ensure no other threads are using the model handle.
     */
    static Result<Model> create_from_buffer(const uint8_t* buffer, size_t buffer_len);

    /**
     * Returns a pointer to the model identifier.
     *
     * The returned string is UTF-8 encoded and null-terminated.
     *
     * @return Model identifier string (UTF-8).
     *
     * @note The returned string is valid for as long as the Model is alive.
     * @warning Not safe against concurrent model destruction.
     */
    std::string get_id() const
    {
        const char* id = aic_model_get_id(model_);
        return id ? std::string(id) : std::string();
    }

    /**
     * Retrieves the optimal sample rate of the model.
     *
     * Each model is optimized for a specific sample rate, which determines the frequency
     * range of the enhanced audio output. While you can process audio at any sample rate,
     * understanding the model's native rate helps predict the enhancement quality.
     *
     * **How sample rate affects enhancement:**
     *
     * - Models trained at lower sample rates (e.g., 8 kHz) can only enhance frequencies
     *   up to their Nyquist limit (4 kHz for 8 kHz models)
     * - When processing higher sample rate input (e.g., 48 kHz) with a lower-rate model,
     *   only the lower frequency components will be enhanced
     *
     * **Enhancement blending:**
     *
     * When enhancement strength is set below 1.0, the enhanced signal is blended with
     * the original, maintaining the full frequency spectrum of your input while adding
     * the model's noise reduction capabilities to the lower frequencies.
     *
     * **Sample rate and optimal frames relationship:**
     *
     * When using different sample rates than the model's native rate, the optimal number
     * of frames (returned by Model::get_optimal_num_frames) will change. The processor's output
     * delay remains constant regardless of sample rate as long as you use the optimal frame
     * count for that rate.
     *
     * **Recommendation:**
     *
     * For maximum enhancement quality across the full frequency spectrum, match your
     * input sample rate to the model's native rate when possible.
     *
     * @return Optimal sample rate in Hz.
     *
     * @note Thread-safe and real-time safe.
     */
    uint32_t get_optimal_sample_rate() const
    {
        uint32_t       sample_rate = 0;
        ::AicErrorCode rc          = aic_model_get_optimal_sample_rate(model_, &sample_rate);
        assert(rc == AIC_ERROR_CODE_SUCCESS);
        (void) rc;
        return sample_rate;
    }

    /**
     * Retrieves the optimal number of frames for the model at a given sample rate.
     *
     * Using the optimal number of frames minimizes latency by avoiding internal buffering.
     *
     * **When you use a different frame count than the optimal value, the processor will
     * introduce additional buffering latency on top of its base processing delay.**
     *
     * The optimal frame count varies based on the sample rate. Each model operates on a
     * fixed time window length, so the required number of frames changes with sample rate.
     * For example, a model designed for 10 ms processing windows requires 480 frames at
     * 48 kHz, but only 160 frames at 16 kHz to capture the same duration of audio.
     *
     * Call this function with your intended sample rate before calling Processor::initialize
     * to determine the best frame count for minimal latency.
     *
     * @param sample_rate Sample rate in Hz for which to calculate the optimal frame count.
     * @return Optimal frame count.
     *
     * @note Thread-safe and real-time safe.
     */
    size_t get_optimal_num_frames(uint32_t sample_rate) const
    {
        size_t         num_frames = 0;
        ::AicErrorCode rc = aic_model_get_optimal_num_frames(model_, sample_rate, &num_frames);
        assert(rc == AIC_ERROR_CODE_SUCCESS);
        (void) rc;
        return num_frames;
    }

  private:
    // Friend declaration: allows Processor to access the raw model handle for creation.
    friend class Processor;

    // Creates an empty Model wrapper for internal use when creation fails.
    Model() : model_(nullptr) {}
    // Wraps an existing SDK model handle; this instance becomes responsible for destroying it.
    explicit Model(::AicModel* model) : model_(model) {}
};

// ---------------------------
// Configuration
// ---------------------------

struct ProcessorConfig
{
    uint32_t sample_rate;
    uint16_t num_channels;
    size_t   num_frames;
    bool     allow_variable_frames;

    /**
     * Constructs a ProcessorConfig with the specified parameters.
     *
     * @param sample_rate Audio sample rate in Hz.
     * @param num_frames Number of frames per processing block.
     * @param num_channels Number of audio channels (1 for mono, 2 for stereo, etc.).
     * @param allow_variable_frames True to enable variable frame sizes, false for fixed size.
     */
    ProcessorConfig(uint32_t sample_rate,
                    size_t   num_frames,
                    uint16_t num_channels          = 1,
                    bool     allow_variable_frames = false)
        : sample_rate(sample_rate)
        , num_channels(num_channels)
        , num_frames(num_frames)
        , allow_variable_frames(allow_variable_frames)
    {}
};

// ---------------------------
// Processor context wrapper
// ---------------------------

class ProcessorContext
{
  private:
    ::AicProcessorContext* context_;

  public:
    // Destructor: releases the underlying SDK processor context handle if one is owned
    ~ProcessorContext()
    {
        if (context_)
        {
            aic_processor_context_destroy(context_);
        }
    }

    // Move constructor: the handle from the source ProcessorContext gets moved into the new
    // ProcessorContext
    ProcessorContext(ProcessorContext&& other) noexcept : context_(other.context_)
    {
        other.context_ = nullptr;
    }

    // Move assignment: replaces the currently owned handle with the source handle and clears the
    // source
    ProcessorContext& operator=(ProcessorContext&& other) noexcept
    {
        if (this != &other)
        {
            if (context_)
            {
                aic_processor_context_destroy(context_);
            }
            context_       = other.context_;
            other.context_ = nullptr;
        }
        return *this;
    }

    // Deleted copy constructor: copying is disabled because this wrapper has unique ownership of
    // the handle
    ProcessorContext(const ProcessorContext&) = delete;

    // Deleted copy assignment: copying is disabled for the same reason as the copy constructor
    ProcessorContext& operator=(const ProcessorContext&) = delete;

    /**
     * Clears all internal state and buffers.
     *
     * Call this when the audio stream is interrupted or when seeking
     * to prevent artifacts from previous audio content.
     *
     * The processor stays initialized to the configured settings.
     *
     * @return ErrorCode::Success on success, or an error code on failure.
     *
     * @note Thread-safe and real-time safe.
     */
    ErrorCode reset() const
    {
        ::AicErrorCode rc = aic_processor_context_reset(context_);
        return static_cast<ErrorCode>(static_cast<int>(rc));
    }

    /**
     * Modifies an enhancement parameter.
     *
     * All parameters can be changed during audio processing.
     * This function can be called from any thread.
     *
     * @param parameter Parameter to modify.
     * @param value New parameter value.
     * @return ErrorCode::Success on success, or an error code on failure.
     *
     * @note Thread-safe and real-time safe.
     */
    ErrorCode set_parameter(ProcessorParameter parameter, float value) const
    {
        ::AicErrorCode rc = aic_processor_context_set_parameter(
            context_, static_cast<::AicProcessorParameter>(static_cast<int>(parameter)), value);
        return static_cast<ErrorCode>(static_cast<int>(rc));
    }

    /**
     * Retrieves the current value of a parameter.
     *
     * This function can be called from any thread.
     *
     * @param parameter Parameter to query.
     * @return Current parameter value.
     *
     * @note Thread-safe and real-time safe.
     */
    float get_parameter(ProcessorParameter parameter) const
    {
        float          value = 0.0f;
        ::AicErrorCode rc    = aic_processor_context_get_parameter(
            context_, static_cast<::AicProcessorParameter>(static_cast<int>(parameter)), &value);
        assert(rc == AIC_ERROR_CODE_SUCCESS);
        (void) rc;
        return value;
    }

    /**
     * Returns the total output delay in samples for the current audio configuration.
     *
     * This function provides the complete end-to-end latency introduced by the processor,
     * which includes both algorithmic processing delay and any buffering overhead.
     * Use this value to synchronize enhanced audio with other streams or to implement
     * delay compensation in your application.
     *
     * @return Output delay in samples.
     *
     * @note Before initialization, returns the base processing delay using the
     *       processor's optimal frame size at its native sample rate. After initialization,
     *       returns the delay for the configured sample rate and frame count.
     * @note The delay value is expressed in samples at the configured sample rate.
     *       To convert to milliseconds: delay_ms = (delay_samples * 1000) / sample_rate.
     * @note Using frame sizes different from the model's optimal value increases delay.
     * @note Thread-safe and real-time safe.
     */
    size_t get_output_delay() const
    {
        size_t         latency = 0;
        ::AicErrorCode rc      = aic_processor_context_get_output_delay(context_, &latency);
        assert(rc == AIC_ERROR_CODE_SUCCESS);
        (void) rc;
        return latency;
    }

  private:
    // Friend declaration: allows Processor to construct ProcessorContext instances from raw handles
    friend class Processor;

    // Constructor: creates an empty context wrapper for internal use when creation fails
    ProcessorContext() : context_(nullptr) {}

    // Constructor: wraps an existing SDK processor context handle; this instance becomes
    // responsible for destroying it
    explicit ProcessorContext(::AicProcessorContext* context) : context_(context) {}
};

// ---------------------------
// VAD context wrapper
// ---------------------------

class VadContext
{
  private:
    ::AicVadContext* context_;

  public:
    // Destructor: releases the underlying SDK VAD context handle if one is owned
    ~VadContext()
    {
        if (context_)
        {
            aic_vad_context_destroy(context_);
        }
    }

    // Move constructor: the handle from the source VadContext gets moved into the new VadContext
    VadContext(VadContext&& other) noexcept : context_(other.context_)
    {
        other.context_ = nullptr;
    }

    // Move assignment: replaces the currently owned handle with the source handle and clears the
    // source
    VadContext& operator=(VadContext&& other) noexcept
    {
        if (this != &other)
        {
            if (context_)
            {
                aic_vad_context_destroy(context_);
            }
            context_       = other.context_;
            other.context_ = nullptr;
        }
        return *this;
    }

    // Deleted copy constructor: copying is disabled because this wrapper has unique ownership of
    // the handle
    VadContext(const VadContext&) = delete;

    // Deleted copy assignment: copying is disabled for the same reason as the copy constructor
    VadContext& operator=(const VadContext&) = delete;

    /**
     * Returns the VAD's prediction.
     *
     * **Important:**
     * - The latency of the VAD prediction is equal to
     *   the backing processor's processing latency.
     * - If the backing processor stops being processed,
     *   the VAD will not update its speech detection prediction.
     *
     * @return True if speech is detected, false otherwise.
     *
     * @note Thread-safe and real-time safe.
     */
    bool is_speech_detected() const
    {
        bool           value = false;
        ::AicErrorCode rc    = aic_vad_context_is_speech_detected(context_, &value);
        assert(rc == AIC_ERROR_CODE_SUCCESS);
        (void) rc;
        return value;
    }

    /**
     * Modifies a VAD parameter.
     *
     * All parameters can be changed during audio processing.
     * This function can be called from any thread.
     *
     * @param parameter Parameter to modify.
     * @param value New parameter value.
     * @return ErrorCode::Success on success, or an error code on failure.
     *
     * @note Thread-safe and real-time safe.
     */
    ErrorCode set_parameter(VadParameter parameter, float value) const
    {
        ::AicErrorCode rc = aic_vad_context_set_parameter(
            context_, static_cast<::AicVadParameter>(static_cast<int>(parameter)), value);
        return static_cast<ErrorCode>(static_cast<int>(rc));
    }

    /**
     * Retrieves the current value of a parameter.
     *
     * This function can be called from any thread.
     *
     * @param parameter Parameter to query.
     * @return Current parameter value.
     *
     * @note Thread-safe and real-time safe.
     */
    float get_parameter(VadParameter parameter) const
    {
        float          value = 0.0f;
        ::AicErrorCode rc    = aic_vad_context_get_parameter(
            context_, static_cast<::AicVadParameter>(static_cast<int>(parameter)), &value);
        assert(rc == AIC_ERROR_CODE_SUCCESS);
        (void) rc;
        return value;
    }

  private:
    // Friend declaration: allows Processor to construct VadContext instances from raw handles
    friend class Processor;

    // Constructor: creates an empty VAD context wrapper for internal use when creation fails
    VadContext() : context_(nullptr) {}
    // Constructor: wraps an existing SDK VAD context handle; this instance becomes responsible for
    // destroying it
    explicit VadContext(::AicVadContext* context) : context_(context) {}
};

// ---------------------------
// Processor wrapper
// ---------------------------

class Processor
{
  private:
    ::AicProcessor* processor_;

  public:
    // Destructor: releases the underlying SDK processor handle if one is owned
    ~Processor()
    {
        if (processor_)
        {
            aic_processor_destroy(processor_);
        }
    }

    // Move constructor: the handle from the source Processor gets moved into the new Processor
    Processor(Processor&& other) noexcept : processor_(other.processor_)
    {
        other.processor_ = nullptr;
    }

    // Move assignment: replaces the currently owned handle with the source handle and clears the
    // source
    Processor& operator=(Processor&& other) noexcept
    {
        if (this != &other)
        {
            if (processor_)
            {
                aic_processor_destroy(processor_);
            }
            processor_       = other.processor_;
            other.processor_ = nullptr;
        }
        return *this;
    }

    // Deleted copy constructor: copying is disabled because this wrapper has unique ownership of
    // the handle
    Processor(const Processor&) = delete;

    // Deleted copy assignment: copying is disabled for the same reason as the copy constructor
    Processor& operator=(const Processor&) = delete;

    /**
     * Creates a new audio processor instance.
     *
     * Multiple processors can be created to process different audio streams simultaneously
     * or to switch between different enhancement algorithms during runtime.
     *
     * @param model Model instance to process.
     * @param license_key SDK license key.
     * @return Result containing the Processor and an ErrorCode.
     *
     * @warning Not thread-safe. Ensure no other threads are using the processor handle.
     */
    static Result<Processor> create(const Model& model, const std::string& license_key);

    /**
     * Configures the processor for a specific audio format.
     *
     * This function must be called before processing any audio.
     * For the lowest delay use the sample rate and frame size returned by
     * Model::get_optimal_sample_rate and Model::get_optimal_num_frames.
     *
     * @param sample_rate Audio sample rate in Hz (8000 - 192000).
     * @param num_channels Number of audio channels (1 for mono, 2 for stereo, etc.).
     * @param num_frames Number of samples per channel in each process call.
     * @param allow_variable_frames Allows varying frame counts per process call (up to num_frames).
     * @return ErrorCode::Success if configuration is accepted, or an error code on failure.
     *
     * @note All channels are mixed to mono for processing. To process channels
     *       independently, create separate processor instances.
     * @warning Allocates memory and is not thread-safe. Avoid calling from real-time audio threads.
     */
    ErrorCode initialize(uint32_t sample_rate, uint16_t num_channels, size_t num_frames,
                         bool allow_variable_frames)
    {
        ::AicErrorCode rc = aic_processor_initialize(processor_, sample_rate, num_channels,
                                                     num_frames, allow_variable_frames);
        return static_cast<ErrorCode>(static_cast<int>(rc));
    }

    /**
     * Processes audio with separate buffers for each channel (planar layout).
     *
     * Enhances speech in the provided audio buffers in-place.
     *
     * **Memory Layout:**
     * - `audio` is an array of pointers, one pointer per channel
     * - Each pointer points to a separate buffer containing `num_frames` samples for that channel
     * - Example for 2 channels, 4 frames:
     *   ```
     *   audio[0] -> [ch0_f0, ch0_f1, ch0_f2, ch0_f3]
     *   audio[1] -> [ch1_f0, ch1_f1, ch1_f2, ch1_f3]
     *   ```
     *
     * The planar function allows a maximum of 16 channels.
     *
     * @param audio Array of channel buffer pointers, one per channel.
     * @param num_channels Number of channels (must match initialization).
     * @param num_frames Number of samples per channel.
     * @return ErrorCode::Success on success, or an error code on failure.
     *
     * @note If allow_variable_frames is enabled, num_frames may be less than or equal
     *       to the initialization value.
     * @warning Real-time safe but not thread-safe; do not call from multiple threads.
     */
    ErrorCode process_planar(float* const* audio, uint16_t num_channels, size_t num_frames)
    {
        ::AicErrorCode rc =
            aic_processor_process_planar(processor_, audio, num_channels, num_frames);
        return static_cast<ErrorCode>(static_cast<int>(rc));
    }

    /**
     * Processes audio with interleaved channels in a single buffer.
     *
     * Enhances speech in the provided audio buffer in-place.
     *
     * **Memory Layout:**
     * - Single contiguous buffer with channels interleaved
     * - Buffer size: `num_channels` * `num_frames` floats
     * - Example for 2 channels, 4 frames:
     *   ```
     *   audio -> [ch0_f0, ch1_f0, ch0_f1, ch1_f1, ch0_f2, ch1_f2, ch0_f3, ch1_f3]
     *   ```
     *
     * @param audio Interleaved audio buffer of size num_channels * num_frames.
     * @param num_channels Number of channels (must match initialization).
     * @param num_frames Number of samples per channel.
     * @return ErrorCode::Success on success, or an error code on failure.
     *
     * @note If allow_variable_frames is enabled, num_frames may be less than or equal
     *       to the initialization value.
     * @warning Real-time safe but not thread-safe; do not call from multiple threads.
     */
    ErrorCode process_interleaved(float* audio, uint16_t num_channels, size_t num_frames)
    {
        ::AicErrorCode rc =
            aic_processor_process_interleaved(processor_, audio, num_channels, num_frames);
        return static_cast<ErrorCode>(static_cast<int>(rc));
    }

    /**
     * Processes audio with sequential channel data in a single buffer.
     *
     * Enhances speech in the provided audio buffer in-place.
     *
     * **Memory Layout:**
     * - Single contiguous buffer with all samples for each channel stored sequentially
     * - Buffer size: `num_channels` * `num_frames` floats
     * - Example for 2 channels, 4 frames:
     *   ```
     *   audio -> [ch0_f0, ch0_f1, ch0_f2, ch0_f3, ch1_f0, ch1_f1, ch1_f2, ch1_f3]
     *   ```
     *
     * @param audio Sequential audio buffer of size num_channels * num_frames.
     * @param num_channels Number of channels (must match initialization).
     * @param num_frames Number of samples per channel.
     * @return ErrorCode::Success on success, or an error code on failure.
     *
     * @note If allow_variable_frames is enabled, num_frames may be less than or equal
     *       to the initialization value.
     * @warning Real-time safe but not thread-safe; do not call from multiple threads.
     */
    ErrorCode process_sequential(float* audio, uint16_t num_channels, size_t num_frames)
    {
        ::AicErrorCode rc =
            aic_processor_process_sequential(processor_, audio, num_channels, num_frames);
        return static_cast<ErrorCode>(static_cast<int>(rc));
    }

    /**
     * Creates a processor context handle for thread-safe control APIs.
     *
     * The returned context can be used to reset the processor, adjust parameters,
     * and query output delay from any thread.
     *
     * @return Result containing the ProcessorContext and an ErrorCode.
     *
     * @note Thread-safe.
     */
    Result<ProcessorContext> create_context() const;

    /**
     * Creates a VAD context handle for thread-safe control APIs.
     *
     * The voice activity detection works automatically using the enhanced audio output
     * of a given processor.
     *
     * **Important:** If the backing processor is destroyed, the VAD instance will stop
     * producing new data. It is safe to destroy the processor without destroying the VAD.
     *
     * @return Result containing the VadContext and an ErrorCode.
     *
     * @note Thread-safe and real-time safe.
     * @note It is safe for the processor to be in use by other threads.
     */
    Result<VadContext> create_vad_context() const;

  private:
    // Constructor: creates an empty Processor wrapper for internal use when creation fails
    Processor() : processor_(nullptr) {}
    // Constructor: wraps an existing SDK processor handle; this instance becomes responsible for
    // destroying it
    explicit Processor(::AicProcessor* processor) : processor_(processor) {}
};

// ---------------------------
// SDK info helpers
// ---------------------------

/**
 * Returns the version of the SDK.
 *
 * @return SDK version string (e.g., "1.2.3").
 *
 * @note Thread-safe and real-time safe.
 */
inline std::string get_sdk_version()
{
    const char* v = ::aic_get_sdk_version();
    return v ? std::string(v) : std::string();
}

/**
 * Returns the model version compatible with the SDK.
 *
 * @return Model version compatible with this SDK build.
 *
 * @note Thread-safe and real-time safe.
 */
inline uint32_t get_compatible_model_version()
{
    return ::aic_get_compatible_model_version();
}

} // namespace aic
