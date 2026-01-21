#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

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
    /// Processor must be initialized before calling this operation. Call `aic_processor_initialize` first.
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
    /// License key has expired
    LicenseExpired = AIC_ERROR_CODE_LICENSE_EXPIRED,
    /// The model file is invalid or corrupted. Verify the file is correct.
    ModelInvalid = AIC_ERROR_CODE_MODEL_INVALID,
    /// The model file version is not compatible with this SDK version.
    ModelVersionUnsupported = AIC_ERROR_CODE_MODEL_VERSION_UNSUPPORTED,
    /// The path to the model file is invalid.
    ModelFilePathInvalid = AIC_ERROR_CODE_MODEL_FILE_PATH_INVALID,
    /// File system error when accessing the model file.
    FileSystemError = AIC_ERROR_CODE_FILE_SYSTEM_ERROR,
    /// Model data is not aligned to 64 bytes.
    ModelDataUnaligned = AIC_ERROR_CODE_MODEL_DATA_UNALIGNED,
};

template <typename T>
struct Result
{
    T         value;
    ErrorCode error;

    // Default constructor: default-constructs the value and sets a non-success error as a safe sentinel
    Result() : value(), error(ErrorCode::InternalError) {}
    // Constructor: copies the value and stores the error code
    Result(const T& v, ErrorCode e) : value(v), error(e) {}
    // Constructor: moves the value and stores the error code
    Result(T&& v, ErrorCode e) : value(std::move(v)), error(e) {}

    bool ok() const { return error == ErrorCode::Success; }
    // Helper: returns the contained value by move (useful for move-only types).
    T take() { return std::move(value); }
};

/// Configurable parameters for audio enhancement
enum class ProcessorParameter : int
{
    /// Bypass keeping processing delay (0.0/1.0): 0.0=disabled, 1.0=enabled
    Bypass = AIC_PROCESSOR_PARAMETER_BYPASS,
    /// Enhancement intensity (0.0-1.0): 0.0=bypass, 1.0=full enhancement
    EnhancementLevel = AIC_PROCESSOR_PARAMETER_ENHANCEMENT_LEVEL,
    /// Voice gain multiplier (0.1-4.0): linear amplitude multiplier
    VoiceGain = AIC_PROCESSOR_PARAMETER_VOICE_GAIN,
};

/// Configurable parameters for voice activity detection (VAD)
enum class VadParameter : int
{
    /// Controls for how long the VAD continues to detect speech after the audio signal no longer contains speech. (0.0 to 20x model window length in seconds)
    SpeechHoldDuration = AIC_VAD_PARAMETER_SPEECH_HOLD_DURATION,
    /// Controls the sensitivity (energy threshold) of the VAD. (1.0-15.0)
    Sensitivity = AIC_VAD_PARAMETER_SENSITIVITY,
    /// Controls for how long speech needs to be present in the audio signal before the VAD considers it speech (0.0 - 1.0 seconds).
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

    // Move assignment: replaces the currently owned handle with the source handle and clears the source
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

    // Deleted copy constructor: copying is disabled because this wrapper has unique ownership of the handle
    Model(const Model&)            = delete;

    // Deleted copy assignment: copying is disabled for the same reason as the copy constructor
    Model& operator=(const Model&) = delete;

    /**
     * Creates a new model instance from a model file.
     *
     * A single model instance can be used to create multiple processors.
     *
     * # Note
     * Processor instances retain a shared reference to the model data.
     * It is safe to destroy the model handle after creating the desired processors.
     * The memory used by the model will be automatically freed after all processors
     * using that model have been destroyed.
     *
     * # Safety
     * - This function is not thread-safe. Ensure no other threads are using the model handle or the same file path.
     *
     * @param file_path NULL-terminated string containing the path to the model file. Must not be NULL.
     * @return `Result` containing the `Model` and an `ErrorCode`.
     */
    static Result<Model> create_from_file(const std::string& file_path);

    /**
     * Creates a new model instance from a memory buffer.
     *
     * The buffer must remain valid and unchanged for the lifetime of the model.
     *
     * # Note
     * Processor instances retain a shared reference to the model data.
     * It is safe to destroy the model handle after creating the desired processors.
     * The memory used by the model will be automatically freed after all processors
     * using that model have been destroyed.
     * 
     * # Safety
     * - This function is not thread-safe. Ensure no other threads are using the model handle.
     *
     * @param buffer Pointer to model bytes (must be 64-byte aligned).
     * @param buffer_len Size of the model buffer in bytes.
     * @return `Result` containing the `Model` and an `ErrorCode`.
     */
    static Result<Model> create_from_buffer(const uint8_t* buffer, size_t buffer_len);

    /**
     * Returns the model identifier string.
     *
     * The returned string is valid for as long as the Model is alive.
     *
     * Thread safety: not safe against concurrent model destruction.
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
     * of frames (returned by `aic_model_get_optimal_num_frames`) will change. The processor's output
     * delay remains constant regardless of sample rate as long as you use the optimal frame
     * count for that rate.
     *
     * **Recommendation:**
     *
     * For maximum enhancement quality across the full frequency spectrum, match your
     * input sample rate to the model's native rate when possible.
     *
     * # Parameters
     * - `model`: Model instance. Must not be NULL.
     * - `sample_rate`: Receives the optimal sample rate in Hz. Must not be NULL.
     *
     * # Returns
     * - `AIC_ERROR_CODE_SUCCESS`: Sample rate retrieved successfully
     * - `AIC_ERROR_CODE_NULL_POINTER`: `model` or `sample_rate` is NULL
     *
     * # Safety
     * - Real-time safe: Can be called from audio processing threads.
     * - Thread-safe: Can be called from any thread.
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
     * Call this function with your intended sample rate before calling `aic_processor_initialize`
     * to determine the best frame count for minimal latency.
     *
     * # Parameters
     * - `model`: Model instance. Must not be NULL.
     * - `sample_rate`: The sample rate in Hz for which to calculate the optimal frame count.
     * - `num_frames`: Receives the optimal frame count. Must not be NULL.
     *
     * # Returns
     * - `AIC_ERROR_CODE_SUCCESS`: Frame count retrieved successfully
     * - `AIC_ERROR_CODE_NULL_POINTER`: `model` or `num_frames` is NULL
     *
     * # Safety
     * - Real-time safe: Can be called from audio processing threads.
     * - Thread-safe: Can be called from any thread.
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
    uint32_t sample_rate           = 0;
    uint16_t num_channels          = 1;
    size_t   num_frames            = 0;
    bool     allow_variable_frames = false;

    /** Returns a [`ProcessorConfig`] pre-filled with the model's optimal sample rate and frame size.
     *
     * `num_channels` will be set to `1` and `allow_variable_frames` to `false`.
     * Adjust the number of channels and enable variable frames by using the builder pattern.
     *
     * If you need to configure a non-optimal sample rate or number of frames,
     * construct the [`ProcessorConfig`] struct directly.
     */
    static ProcessorConfig optimal(const Model& model)
    {
        ProcessorConfig config;
        config.sample_rate           = model.get_optimal_sample_rate();
        config.num_frames            = model.get_optimal_num_frames(config.sample_rate);
        config.num_channels          = 1;
        config.allow_variable_frames = false;
        return config;
    }

    /** Sets the number of audio channels for processing.
     *
     * # Arguments
     *
     * - `num_channels` - Number of audio channels (1 for mono, 2 for stereo, etc.)
     */
    ProcessorConfig with_num_channels(uint16_t channels) const
    {
        ProcessorConfig copy = *this;
        copy.num_channels    = channels;
        return copy;
    }

    /** Enables or disables variable frame size support.
     *
     * When enabled, allows processing frame counts below `num_frames` at the cost of added latency.
     *
     * # Arguments
     *
     * - `allow_variable_frames` - `true` to enable variable frame sizes, `false` for fixed size
     */
    ProcessorConfig with_allow_variable_frames(bool allow) const
    {
        ProcessorConfig copy       = *this;
        copy.allow_variable_frames = allow;
        return copy;
    }
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

    // Move constructor: the handle from the source ProcessorContext gets moved into the new ProcessorContext
    ProcessorContext(ProcessorContext&& other) noexcept : context_(other.context_)
    {
        other.context_ = nullptr;
    }

    // Move assignment: replaces the currently owned handle with the source handle and clears the source
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

    // Deleted copy constructor: copying is disabled because this wrapper has unique ownership of the handle
    ProcessorContext(const ProcessorContext&)            = delete;

    // Deleted copy assignment: copying is disabled for the same reason as the copy constructor
    ProcessorContext& operator=(const ProcessorContext&) = delete;

    /**
     * Clears all internal state and buffers.
     *
     * Call this when the audio stream is interrupted or when seeking
     * to prevent artifacts from previous audio content.
     *
     * This operates on the processor associated with the provided context handle.
     *
     * The processor stays initialized to the configured settings.
     *
     * # Parameters
     * - `context`: Processor context instance to reset. Must not be NULL.
     *
     * # Returns
     * - `AIC_ERROR_CODE_SUCCESS`: State cleared successfully
     * - `AIC_ERROR_CODE_NULL_POINTER`: `context` is NULL
     *
     * # Safety
     * - Real-time safe: Can be called from audio processing threads.
     * - Thread-safe: Can be called from any thread.
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
     * This operates on the processor associated with the provided context handle.
     *
     * # Parameters
     * - `context`: Processor context instance. Must not be NULL.
     * - `parameter`: Parameter to modify.
     * - `value`: New parameter value. See parameter documentation for ranges.
     *
     * # Returns
     * - `AIC_ERROR_CODE_SUCCESS`: Parameter updated successfully
     * - `AIC_ERROR_CODE_NULL_POINTER`: `context` is NULL
     * - `AIC_ERROR_CODE_PARAMETER_OUT_OF_RANGE`: Value outside valid range
     *
     * # Safety
     * - Real-time safe: Can be called from audio processing threads.
     * - Thread-safe: Can be called from any thread.
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
     * This queries the processor associated with the provided context handle.
     *
     * # Parameters
     * - `context`: Processor context instance. Must not be NULL.
     * - `parameter`: Parameter to query.
     * - `value`: Receives the current parameter value. Must not be NULL.
     *
     * # Returns
     * - `AIC_ERROR_CODE_SUCCESS`: Parameter retrieved successfully
     * - `AIC_ERROR_CODE_NULL_POINTER`: `context` or `value` is NULL
     *
     * # Safety
     * - Real-time safe: Can be called from audio processing threads.
     * - Thread-safe: Can be called from any thread.
     */
    float get_parameter(ProcessorParameter parameter) const
    {
        float          value = 0.0f;
        ::AicErrorCode rc = aic_processor_context_get_parameter(
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
     * This queries the processor associated with the provided context handle.
     *
     * **Delay behavior:**
     * - **Before initialization:** Returns the base processing delay using the processor's
     *   optimal frame size at its native sample rate
     * - **After initialization:** Returns the actual delay for your specific configuration,
     *   including any additional buffering introduced by non-optimal frame sizes
     *
     * **Important:** The delay value is always expressed in samples at the sample rate
     * you configured during `aic_processor_initialize`. To convert to time units:
     * `delay_ms = (delay_samples * 1000) / sample_rate`
     *
     * **Note:** Using frame sizes different from the optimal value returned by
     * `aic_processor_get_optimal_num_frames` will increase the delay beyond the processor's base latency.
     *
     * # Parameters
     * - `context`: Processor context instance. Must not be NULL.
     * - `delay`: Receives the delay in samples. Must not be NULL.
     *
     * # Returns
     * - `AIC_ERROR_CODE_SUCCESS`: Delay retrieved successfully
     * - `AIC_ERROR_CODE_NULL_POINTER`: `context` or `delay` is NULL
     *
     * # Safety
     * - Real-time safe: Can be called from audio processing threads.
     * - Thread-safe: Can be called from any thread.
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
    
    // Constructor: wraps an existing SDK processor context handle; this instance becomes responsible for destroying it
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

    // Move assignment: replaces the currently owned handle with the source handle and clears the source
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

    // Deleted copy constructor: copying is disabled because this wrapper has unique ownership of the handle
    VadContext(const VadContext&)            = delete;

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
     * # Parameters
     * - `context`: VAD context instance. Must not be NULL.
     * - `value`: Receives the VAD prediction. Must not be NULL.
     *
     * # Returns
     * - `AIC_ERROR_CODE_SUCCESS`: Prediction retrieved successfully
     * - `AIC_ERROR_CODE_NULL_POINTER`: `context` or `value` is NULL
     *
     * # Safety
     * - Real-time safe: Can be called from audio processing threads.
     * - Thread-safe: Can be called from any thread.
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
     * # Parameters
     * - `context`: VAD context instance. Must not be NULL.
     * - `parameter`: Parameter to modify.
     * - `value`: New parameter value. See parameter documentation for ranges.
     *
     * # Returns
     * - `AIC_ERROR_CODE_SUCCESS`: Parameter updated successfully
     * - `AIC_ERROR_CODE_NULL_POINTER`: `context` is NULL
     * - `AIC_ERROR_CODE_PARAMETER_OUT_OF_RANGE`: Value outside valid range
     *
     * # Safety
     * - Real-time safe: Can be called from audio processing threads.
     * - Thread-safe: Can be called from any thread.
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
     * # Parameters
     * - `context`: VAD context instance. Must not be NULL.
     * - `parameter`: Parameter to query.
     * - `value`: Receives the current parameter value. Must not be NULL.
     *
     * # Returns
     * - `AIC_ERROR_CODE_SUCCESS`: Parameter retrieved successfully
     * - `AIC_ERROR_CODE_NULL_POINTER`: `context` or `value` is NULL
     *
     * # Safety
     * - Real-time safe: Can be called from audio processing threads.
     * - Thread-safe: Can be called from any thread.
     */
    float get_parameter(VadParameter parameter) const
    {
        float          value = 0.0f;
        ::AicErrorCode rc = aic_vad_context_get_parameter(
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
    // Constructor: wraps an existing SDK VAD context handle; this instance becomes responsible for destroying it
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

    // Move assignment: replaces the currently owned handle with the source handle and clears the source
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

    // Deleted copy constructor: copying is disabled because this wrapper has unique ownership of the handle
    Processor(const Processor&)            = delete;

    // Deleted copy assignment: copying is disabled for the same reason as the copy constructor
    Processor& operator=(const Processor&) = delete;

    /**
     * Creates a new audio processor instance.
     *
     * Multiple processors can be created to process different audio streams simultaneously
     * or to switch between different enhancement algorithms during runtime.
     *
     * # Parameters
     * - `processor`: Receives the handle to the newly created processor. Must not be NULL.
     * - `model`: Handle to the model instance to process. Must not be NULL.
     * - `license_key`: NULL-terminated string containing your license key. Must not be NULL.
     *
     * # Returns
     * - `AIC_ERROR_CODE_SUCCESS`: Processor created successfully
     * - `AIC_ERROR_CODE_NULL_POINTER`: `processor` or `model` or `license_key` is NULL
     * - `AIC_ERROR_CODE_LICENSE_INVALID`: License key format is incorrect
     * - `AIC_ERROR_CODE_LICENSE_VERSION_UNSUPPORTED`: License version is not compatible with the SDK version
     * - `AIC_ERROR_CODE_LICENSE_EXPIRED`: License key has expired
     *
     * # Safety
     * - This function is not thread-safe. Ensure no other threads are using the processor handle.
     */
    static Result<Processor> create(const Model& model, const std::string& license_key);

    /**
     * Configures the processor for a specific audio format.
     *
     * This function must be called before processing any audio.
     * For the lowest delay use the sample rate and frame size returned by
     * `aic_processor_get_optimal_sample_rate` and `aic_processor_get_optimal_num_frames`.
     *
     * # Parameters
     * - `processor`: Processor instance to configure. Must not be NULL.
     * - `sample_rate`: Audio sample rate in Hz (8000 - 192000).
     * - `num_channels`: Number of audio channels (1 for mono, 2 for stereo, etc.).
     * - `num_frames`: Number of samples per channel in each process call.
     * - `allow_variable_frames`: Allows varying frame counts per process call (up to `num_frames`), but increases delay.
     *
     * # Returns
     * - `AIC_ERROR_CODE_SUCCESS`: Configuration accepted
     * - `AIC_ERROR_CODE_NULL_POINTER`: `processor` is NULL
     * - `AIC_ERROR_CODE_UNSUPPORTED_AUDIO_CONFIG`: Configuration is not supported
     *
     * # Note
     * All channels are mixed to mono for processing. To process channels
     * independently, create separate processor instances.
     *
     * # Safety
     * - This function allocates memory. Avoid calling it from real-time audio threads.
     * - This function is not thread-safe. Ensure no other threads are using the processor during initialization.
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
     * # Parameters
     * - `processor`: Initialized processor instance. Must not be NULL.
     * - `audio`: Array of `num_channels` pointers, each pointing to a buffer of `num_frames` floats. Must not be NULL.
     * - `num_channels`: Number of channels (must match initialization).
     * - `num_frames`: Number of samples per channel (must match initialization value, or if `allow_variable_frames` was enabled, must be ≤ initialization value).
     *
     * # Returns
     * - `AIC_ERROR_CODE_SUCCESS`: Audio processed successfully
     * - `AIC_ERROR_CODE_NULL_POINTER`: `processor` or `audio` is NULL
     * - `AIC_ERROR_CODE_NOT_INITIALIZED`: Processor has not been initialized
     * - `AIC_ERROR_CODE_AUDIO_CONFIG_MISMATCH`: Channel or frame count mismatch
     * - `AIC_ERROR_CODE_ENHANCEMENT_NOT_ALLOWED`: SDK key was not authorized or process failed to report usage. Check if you have internet connection.
     *
     * # Safety
     * - Real-time safe: Can be called from audio processing threads.
     * - This function is not thread-safe. Do not call this function from multiple threads.
     */
    ErrorCode process_planar(float* const* audio, uint16_t num_channels, size_t num_frames)
    {
        ::AicErrorCode rc = aic_processor_process_planar(processor_, audio, num_channels, num_frames);
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
     * # Parameters
     * - `processor`: Initialized processor instance. Must not be NULL.
     * - `audio`: Single buffer containing interleaved audio data of size `num_channels` * `num_frames`. Must not be NULL.
     * - `num_channels`: Number of channels (must match initialization).
     * - `num_frames`: Number of samples per channel (must match initialization value, or if `allow_variable_frames` was enabled, must be ≤ initialization value).
     *
     * # Returns
     * - `AIC_ERROR_CODE_SUCCESS`: Audio processed successfully
     * - `AIC_ERROR_CODE_NULL_POINTER`: `processor` or `audio` is NULL
     * - `AIC_ERROR_CODE_NOT_INITIALIZED`: Processor has not been initialized
     * - `AIC_ERROR_CODE_AUDIO_CONFIG_MISMATCH`: Channel or frame count mismatch
     * - `AIC_ERROR_CODE_ENHANCEMENT_NOT_ALLOWED`: SDK key was not authorized or process failed to report usage. Check if you have internet connection.
     *
     * # Safety
     * - Real-time safe: Can be called from audio processing threads.
     * - This function is not thread-safe. Do not call this function from multiple threads.
     */
    ErrorCode process_interleaved(float* audio, uint16_t num_channels, size_t num_frames)
    {
        ::AicErrorCode rc = aic_processor_process_interleaved(processor_, audio, num_channels, num_frames);
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
     * # Parameters
     * - `processor`: Initialized processor instance. Must not be NULL.
     * - `audio`: Single buffer containing sequential audio data of size `num_channels` * `num_frames`. Must not be NULL.
     * - `num_channels`: Number of channels (must match initialization).
     * - `num_frames`: Number of samples per channel (must match initialization value, or if `allow_variable_frames` was enabled, must be ≤ initialization value).
     *
     * # Returns
     * - `AIC_ERROR_CODE_SUCCESS`: Audio processed successfully
     * - `AIC_ERROR_CODE_NULL_POINTER`: `processor` or `audio` is NULL
     * - `AIC_ERROR_CODE_NOT_INITIALIZED`: Processor has not been initialized
     * - `AIC_ERROR_CODE_AUDIO_CONFIG_MISMATCH`: Channel or frame count mismatch
     * - `AIC_ERROR_CODE_ENHANCEMENT_NOT_ALLOWED`: SDK key was not authorized or process failed to report usage. Check if you have internet connection.
     *
     * # Safety
     * - Real-time safe: Can be called from audio processing threads.
     * - This function is not thread-safe. Do not call this function from multiple threads.
     */
    ErrorCode process_sequential(float* audio, uint16_t num_channels, size_t num_frames)
    {
        ::AicErrorCode rc = aic_processor_process_sequential(processor_, audio, num_channels, num_frames);
        return static_cast<ErrorCode>(static_cast<int>(rc));
    }

    /**
     * Creates a processor context handle for thread-safe control APIs.
     *
     * Use the returned handle to reset the processor, parameter APIs,
     * and other thread-safe functions that operate on `AicProcessorContext`.
     *
     * # Parameters
     * - `context`: Receives the handle to the processor context. Must not be NULL.
     * - `processor`: Processor instance. Must not be NULL.
     *
     * # Returns
     * - `AIC_ERROR_CODE_SUCCESS`: Context handle created successfully
     * - `AIC_ERROR_CODE_NULL_POINTER`: `processor` or `context` is NULL
     *
     * # Safety
     * - Thread-safe: Can be called from any thread.
     */
    Result<ProcessorContext> create_context() const;

    /**
     * Creates a VAD context handle for thread-safe control APIs.
     *
     * The voice activity detection works automatically using the enhanced audio output
     * of a given processor.
     *
     * This uses the processor associated with the provided processor handle.
     *
     * **Important:** If the backing processor is destroyed, the VAD instance will stop
     * producing new data. It is safe to destroy the processor without destroying the VAD.
     *
     * # Parameters
     * - `context`: VAD context instance. Must not be NULL.
     * - `processor`: Processor instance to use as data source for the VAD.
     *
     * # Returns
     * - `AIC_ERROR_CODE_SUCCESS`: VAD created successfully
     * - `AIC_ERROR_CODE_NULL_POINTER`: `context` or `processor` is NULL
     *
     * # Safety
     * - Real-time safe: Can be called from audio processing threads.
     * - Thread-safe: Can be called from any thread.
     * - It is safe for the processor handle to be currently in use by other threads.
     */
    Result<VadContext> create_vad_context() const;

  private:
    // Constructor: creates an empty Processor wrapper for internal use when creation fails
    Processor() : processor_(nullptr) {}
    // Constructor: wraps an existing SDK processor handle; this instance becomes responsible for destroying it
    explicit Processor(::AicProcessor* processor) : processor_(processor) {}

};

// ---------------------------
// SDK info helpers
// ---------------------------

/**
 * Returns the version of the SDK.
 *
 * # Returns
 * A null-terminated C string containing the version (e.g., "1.2.3")
 *
 * # Safety
 * - The returned pointer points to a static string and remains valid
 *   for the lifetime of the program. The caller should NOT free this pointer.
 * - Real-time safe: Can be called from audio processing threads.
 * - Thread-safe: Can be called from any thread.
 */
inline std::string get_sdk_version()
{
    const char* v = ::aic_get_sdk_version();
    return v ? std::string(v) : std::string();
}

/**
 * Returns the model version compatible with the SDK.
 *
 * # Returns
 * Model version compatible with this version of the SDK.
 *
 * # Safety
 * - Real-time safe: Can be called from audio processing threads.
 * - Thread-safe: Can be called from any thread.
 */
inline uint32_t get_compatible_model_version()
{
    return ::aic_get_compatible_model_version();
}

} // namespace aic
