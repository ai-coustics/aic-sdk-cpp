# string(JSON) is used to resolve the model from the artifact manifest.
cmake_minimum_required(VERSION 3.19)
project(aic-sdk-cpp-tests LANGUAGES C CXX)

# Export compile commands for vscode, vim, zed etc.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# AudioFile requires C++17. The wrapper itself stays C++11, this only raises the standard for the
# test executable.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# All ai-coustics CMake configuration settings
set(AIC_SDK_ALLOW_DOWNLOAD ON CACHE BOOL "Allow download ai-coustics C SDK")
set(AIC_SDK_USE_STATIC ON CACHE BOOL "Use static linking for AIC SDK")
set(AIC_SDK_ROOT "" CACHE PATH "Path to local ai-coustics C SDK (always skips download if set)")

# Path to local C++ wrapper
set(AIC_SDK_CPP_PATH "../" CACHE PATH "Path to local aic-sdk-cpp directory")

# The model is downloaded at configure time. AIC_TEST_MODEL_ID also selects the fixture directory
# holding the reference output, and AIC_TEST_MODEL_VERSION must match the model version reported by
# aic_get_compatible_model_version. Set AIC_TEST_MODEL to use a local model file instead.
set(AIC_TEST_MODEL_ID "quail-vf-2.2-s-16khz" CACHE STRING "Model used by the end-to-end test")
set(AIC_TEST_MODEL_VERSION "v6" CACHE STRING "Model version compatible with this SDK release")
set(AIC_TEST_MODEL "" CACHE FILEPATH "Local model file to use instead of downloading")

# Same idea as AIC_TEST_MODEL_ID/AIC_TEST_MODEL, but for the standalone VAD end-to-end test. Shares
# AIC_TEST_MODEL_VERSION since both models must be compatible with the same SDK release.
set(AIC_TEST_VAD_MODEL_ID "vad-2.1-xxs-16khz" CACHE STRING "Model used by the end-to-end VAD test")
set(AIC_TEST_VAD_MODEL "" CACHE FILEPATH "Local VAD model file to use instead of downloading")

set(AIC_ARTIFACTS_URL "https://artifacts.ai-coustics.io/")

# Resolves a model ID through the public manifest and downloads it. Model filenames carry a content
# hash, so they cannot be hardcoded. The download is skipped when the file is already present with
# the expected checksum.
function(aic_download_model model_id version out_var)
    set(manifest "${CMAKE_CURRENT_BINARY_DIR}/manifest.json")
    file(DOWNLOAD "${AIC_ARTIFACTS_URL}manifest.json" "${manifest}" STATUS status)
    list(GET status 0 status_code)
    if(NOT status_code EQUAL 0)
        list(GET status 1 status_message)
        message(FATAL_ERROR "Failed to download the model manifest: ${status_message}")
    endif()

    file(READ "${manifest}" manifest_json)
    string(JSON entry ERROR_VARIABLE entry_error
           GET "${manifest_json}" "models" "${model_id}" "versions" "${version}")
    if(entry_error)
        message(FATAL_ERROR "No ${version} release of model ${model_id}: ${entry_error}")
    endif()

    string(JSON model_file GET "${entry}" "file")
    string(JSON model_name GET "${entry}" "filename")
    string(JSON model_hash GET "${entry}" "checksum")

    set(destination "${CMAKE_CURRENT_BINARY_DIR}/models/${model_name}")
    message(STATUS "Model ${model_id} ${version} -> ${destination}")
    file(DOWNLOAD "${AIC_ARTIFACTS_URL}${model_file}" "${destination}"
         EXPECTED_HASH "SHA256=${model_hash}" STATUS status)
    list(GET status 0 status_code)
    if(NOT status_code EQUAL 0)
        list(GET status 1 status_message)
        message(FATAL_ERROR "Failed to download model ${model_id}: ${status_message}")
    endif()

    set(${out_var} "${destination}" PARENT_SCOPE)
endfunction()

include(FetchContent)

# Header-only WAV reader. Its own tests and examples are not needed here.
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    AudioFile
    GIT_REPOSITORY https://github.com/adamstark/AudioFile.git
    GIT_TAG        1.1.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(AudioFile)

if(AIC_SDK_CPP_PATH AND EXISTS "${AIC_SDK_CPP_PATH}")
    # Use the local C++ wrapper
    add_subdirectory("${AIC_SDK_CPP_PATH}" aic_sdk)
else()
    # Download the C++ wrapper
    FetchContent_Declare(
        aic_sdk
        GIT_REPOSITORY https://github.com/ai-coustics/aic-sdk-cpp.git
        GIT_TAG        0.22.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(aic_sdk)
endif()

enable_testing()

add_executable(aic-sdk-e2e-test e2e_test.cpp)
add_executable(aic-sdk-vad-e2e-test vad_e2e_test.cpp)

target_link_libraries(aic-sdk-e2e-test PRIVATE aic-sdk AudioFile)
target_link_libraries(aic-sdk-vad-e2e-test PRIVATE aic-sdk AudioFile)

if(APPLE)
    target_link_libraries(aic-sdk-e2e-test PRIVATE "-framework Security" "-framework CoreFoundation")
    target_link_libraries(aic-sdk-vad-e2e-test PRIVATE "-framework Security" "-framework CoreFoundation")
endif()

if(WIN32)
    target_link_libraries(aic-sdk-e2e-test PRIVATE crypt32)
    target_link_libraries(aic-sdk-vad-e2e-test PRIVATE crypt32)
endif()

if(NOT AIC_TEST_MODEL)
    aic_download_model("${AIC_TEST_MODEL_ID}" "${AIC_TEST_MODEL_VERSION}" AIC_TEST_MODEL)
endif()

if(NOT AIC_TEST_VAD_MODEL)
    aic_download_model("${AIC_TEST_VAD_MODEL_ID}" "${AIC_TEST_MODEL_VERSION}" AIC_TEST_VAD_MODEL)
endif()

set(AIC_TEST_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/test_signal.wav")
set(AIC_TEST_REFERENCE
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/${AIC_TEST_MODEL_ID}/test_signal_enhanced.wav")

if(NOT EXISTS "${AIC_TEST_REFERENCE}")
    message(FATAL_ERROR
        "No reference output for AIC_TEST_MODEL_ID=${AIC_TEST_MODEL_ID}: "
        "expected ${AIC_TEST_REFERENCE}")
endif()

set(AIC_TEST_VAD_REFERENCE
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/${AIC_TEST_VAD_MODEL_ID}/vad_results.json")

if(NOT EXISTS "${AIC_TEST_VAD_REFERENCE}")
    message(FATAL_ERROR
        "No reference output for AIC_TEST_VAD_MODEL_ID=${AIC_TEST_VAD_MODEL_ID}: "
        "expected ${AIC_TEST_VAD_REFERENCE}")
endif()

add_test(NAME enhancement_full_file
         COMMAND aic-sdk-e2e-test "${AIC_TEST_MODEL}" "${AIC_TEST_INPUT}" "${AIC_TEST_REFERENCE}")

add_test(NAME vad_process_blocks
         COMMAND aic-sdk-vad-e2e-test "${AIC_TEST_VAD_MODEL}" "${AIC_TEST_INPUT}"
                 "${AIC_TEST_VAD_REFERENCE}")
