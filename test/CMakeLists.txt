cmake_minimum_required(VERSION 3.14)
project(aic-sdk-tests LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# AudioFile 1.1.4 requires C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -------- ai-coustics SDK --------
set(AIC_SDK_ALLOW_DOWNLOAD ON CACHE BOOL "Allow download ai-coustics C SDK")
set(AIC_SDK_USE_STATIC ON CACHE BOOL "Use static linking for AIC SDK")
set(AIC_SDK_ROOT "" CACHE PATH "Path to local ai-coustics C SDK")

set(AIC_SDK_CPP_PATH "../" CACHE PATH "Path to local aic-sdk-cpp directory")

if(AIC_SDK_CPP_PATH AND EXISTS "${AIC_SDK_CPP_PATH}")
    add_subdirectory("${AIC_SDK_CPP_PATH}" aic_sdk)
else()
    include(FetchContent)
    FetchContent_Declare(
        aic_sdk
        GIT_REPOSITORY https://github.com/ai-coustics/aic-sdk-cpp.git
        GIT_TAG        0.15.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(aic_sdk)
endif()

# -------- Catch2 v3 (test framework) --------
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.13.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

# -------- AudioFile (header-only WAV reader) --------
FetchContent_Declare(
    AudioFile
    GIT_REPOSITORY https://github.com/adamstark/AudioFile.git
    GIT_TAG        1.1.4
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(AudioFile)
if(NOT audiofile_POPULATED)
    FetchContent_Populate(AudioFile)
endif()

# -------- Test executable --------
add_executable(aic_tests test_processor.cpp)

target_link_libraries(aic_tests PRIVATE
    aic-sdk
    Catch2::Catch2WithMain
)
target_include_directories(aic_tests PRIVATE ${audiofile_SOURCE_DIR})

if(APPLE)
    target_link_libraries(aic_tests PRIVATE "-framework Security" "-framework CoreFoundation")
endif()

if(WIN32)
    target_link_libraries(aic_tests PRIVATE crypt32)
endif()

# -------- CTest integration --------
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(aic_tests
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
