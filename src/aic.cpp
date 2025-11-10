#include "aic.hpp"

extern "C" void aic_set_sdk_wrapper_id(uint32_t id);

namespace aic
{

std::pair<std::unique_ptr<AicModel>, ErrorCode>
AicModel::create(ModelType model_type, const std::string& license_key)
{
    static const bool wrapper_id_set = []() {
        aic_set_sdk_wrapper_id(1);
        return true;
    }();
    (void) wrapper_id_set;

    ::AicModel*    raw_model = nullptr;
    ::AicErrorCode rc        = aic_model_create(&raw_model, to_c(model_type), license_key.c_str());

    if (rc == AIC_ERROR_CODE_SUCCESS)
    {
        return {std::unique_ptr<AicModel>(new AicModel(raw_model)), ErrorCode::Success};
    }

    return {std::unique_ptr<AicModel>(), to_cpp(rc)};
}

std::pair<std::unique_ptr<AicVad>, ErrorCode>
AicVad::create(const AicModel& model)
{
    ::AicVad*      raw_vad = nullptr;
    ::AicErrorCode rc      = aic_vad_create(&raw_vad, model.get_c_model());

    if (rc == AIC_ERROR_CODE_SUCCESS)
    {
        return {std::unique_ptr<AicVad>(new AicVad(raw_vad)), ErrorCode::Success};
    }

    return {std::unique_ptr<AicVad>(), to_cpp(rc)};
}

} // namespace aic
