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
     * Creates a new model instance from a file path.
     */
    static Result<Model> create_from_file(const std::string& file_path);

    /**
     * Creates a new model instance from a memory buffer.
     *
     * The buffer must remain valid and 64-byte aligned for the lifetime of the model.
     */
    static Result<Model> create_from_buffer(const uint8_t* buffer, size_t buffer_len);

    /**
     * Returns the model identifier string.
     */
    std::string get_id() const
    {
        const char* id = aic_model_get_id(model_);
        return id ? std::string(id) : std::string();
    }

    /**
     * Retrieves the native sample rate of the selected model.
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
     * Retrieves the optimal number of frames for the selected model and sample rate.
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

    static ProcessorConfig optimal(const Model& model)
    {
        ProcessorConfig config;
        config.sample_rate           = model.get_optimal_sample_rate();
        config.num_frames            = model.get_optimal_num_frames(config.sample_rate);
        config.num_channels          = 1;
        config.allow_variable_frames = false;
        return config;
    }

    ProcessorConfig with_num_channels(uint16_t channels) const
    {
        ProcessorConfig copy = *this;
        copy.num_channels    = channels;
        return copy;
    }

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
     */
    ErrorCode reset() const
    {
        ::AicErrorCode rc = aic_processor_context_reset(context_);
        return static_cast<ErrorCode>(static_cast<int>(rc));
    }

    /**
     * Modifies a processor parameter.
     */
    ErrorCode set_parameter(ProcessorParameter parameter, float value) const
    {
        ::AicErrorCode rc = aic_processor_context_set_parameter(
            context_, static_cast<::AicProcessorParameter>(static_cast<int>(parameter)), value);
        return static_cast<ErrorCode>(static_cast<int>(rc));
    }

    /**
     * Retrieves the current value of a parameter.
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
     * Returns the VAD's speech detection prediction.
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
     */
    ErrorCode set_parameter(VadParameter parameter, float value) const
    {
        ::AicErrorCode rc = aic_vad_context_set_parameter(
            context_, static_cast<::AicVadParameter>(static_cast<int>(parameter)), value);
        return static_cast<ErrorCode>(static_cast<int>(rc));
    }

    /**
     * Retrieves the current value of a VAD parameter.
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
     * Creates a new audio enhancement processor instance.
     */
    static Result<Processor> create(const Model& model, const std::string& license_key);

    /**
     * Configures the processor for a specific audio format.
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
     */
    ErrorCode process_planar(float* const* audio, uint16_t num_channels, size_t num_frames)
    {
        ::AicErrorCode rc = aic_processor_process_planar(processor_, audio, num_channels, num_frames);
        return static_cast<ErrorCode>(static_cast<int>(rc));
    }

    /**
     * Processes audio with interleaved channel data.
     */
    ErrorCode process_interleaved(float* audio, uint16_t num_channels, size_t num_frames)
    {
        ::AicErrorCode rc = aic_processor_process_interleaved(processor_, audio, num_channels, num_frames);
        return static_cast<ErrorCode>(static_cast<int>(rc));
    }

    /**
     * Processes audio with sequential channel data (non-interleaved).
     */
    ErrorCode process_sequential(float* audio, uint16_t num_channels, size_t num_frames)
    {
        ::AicErrorCode rc = aic_processor_process_sequential(processor_, audio, num_channels, num_frames);
        return static_cast<ErrorCode>(static_cast<int>(rc));
    }

    /**
     * Creates a processor context handle for thread-safe control APIs.
     */
    Result<ProcessorContext> create_context() const;

    /**
     * Creates a VAD context handle for thread-safe control APIs.
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
 */
inline std::string get_sdk_version()
{
    const char* v = ::aic_get_sdk_version();
    return v ? std::string(v) : std::string();
}

/**
 * Returns the model version compatible with the SDK.
 */
inline uint32_t get_compatible_model_version()
{
    return ::aic_get_compatible_model_version();
}

} // namespace aic
