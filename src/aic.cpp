#include "aic.hpp"

extern "C" void aic_set_sdk_wrapper_id(uint32_t id);

namespace aic
{

Result<Model> Model::create_from_file(const std::string& file_path)
{
    ::AicModel*    raw_model = nullptr;
    ::AicErrorCode rc        = aic_model_create_from_file(&raw_model, file_path.c_str());

    if (rc == AIC_ERROR_CODE_SUCCESS)
    {
        return Result<Model>(Model(raw_model), ErrorCode::Success);
    }

    return Result<Model>(Model(), static_cast<ErrorCode>(static_cast<int>(rc)));
}

Result<Model> Model::create_from_buffer(const uint8_t* buffer, size_t buffer_len)
{
    ::AicModel*    raw_model = nullptr;
    ::AicErrorCode rc        = aic_model_create_from_buffer(&raw_model, buffer, buffer_len);

    if (rc == AIC_ERROR_CODE_SUCCESS)
    {
        return Result<Model>(Model(raw_model), ErrorCode::Success);
    }

    return Result<Model>(Model(), static_cast<ErrorCode>(static_cast<int>(rc)));
}

Result<Processor> Processor::create(const Model&       model,
                                    const std::string& license_key,
                                    const OtelConfig*  otel_config)
{
    aic_set_sdk_wrapper_id(1);

    ::AicOtelConfig        c_otel{};
    const ::AicOtelConfig* c_otel_ptr = nullptr;
    if (otel_config)
    {
        c_otel.enable             = otel_config->enable;
        c_otel.session_id         = otel_config->session_id;
        c_otel.export_interval_ms = otel_config->export_interval_ms;
        c_otel_ptr                = &c_otel;
    }

    ::AicProcessor* raw_processor = nullptr;
    ::AicErrorCode  rc =
        aic_processor_create(&raw_processor, model.model_, license_key.c_str(), c_otel_ptr);

    if (rc == AIC_ERROR_CODE_SUCCESS)
    {
        return Result<Processor>(Processor(raw_processor), ErrorCode::Success);
    }

    return Result<Processor>(Processor(), static_cast<ErrorCode>(static_cast<int>(rc)));
}

Result<ProcessorContext> Processor::create_context() const
{
    ::AicProcessorContext* raw_context = nullptr;
    ::AicErrorCode         rc          = aic_processor_context_create(&raw_context, processor_);

    if (rc == AIC_ERROR_CODE_SUCCESS)
    {
        return Result<ProcessorContext>(ProcessorContext(raw_context), ErrorCode::Success);
    }

    return Result<ProcessorContext>(ProcessorContext(),
                                    static_cast<ErrorCode>(static_cast<int>(rc)));
}

Result<VadContext> Processor::create_vad_context() const
{
    ::AicVadContext* raw_context = nullptr;
    ::AicErrorCode   rc          = aic_vad_context_create(&raw_context, processor_);

    if (rc == AIC_ERROR_CODE_SUCCESS)
    {
        return Result<VadContext>(VadContext(raw_context), ErrorCode::Success);
    }

    return Result<VadContext>(VadContext(), static_cast<ErrorCode>(static_cast<int>(rc)));
}

Result<AnalyzerPair> AnalyzerPair::create(const Model& model, const std::string& license_key)
{
    aic_set_sdk_wrapper_id(1);

    ::AicCollector* raw_collector = nullptr;
    ::AicAnalyzer*  raw_analyzer  = nullptr;
    ::AicErrorCode  rc            = aic_analyzer_pair_create(&raw_collector, &raw_analyzer,
                                                             model.model_, license_key.c_str());

    if (rc == AIC_ERROR_CODE_SUCCESS)
    {
        return Result<AnalyzerPair>(AnalyzerPair{Collector(raw_collector), Analyzer(raw_analyzer)},
                                    ErrorCode::Success);
    }

    return Result<AnalyzerPair>(AnalyzerPair{Collector(), Analyzer()},
                                static_cast<ErrorCode>(static_cast<int>(rc)));
}

} // namespace aic
