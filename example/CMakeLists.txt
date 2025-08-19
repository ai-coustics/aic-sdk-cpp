cmake_minimum_required(VERSION 3.14)
project(MyProject LANGUAGES C CXX)

# Export compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Require C++11 for the example app
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# Optional: Choose static or dynamic linking (default is static)
set(AIC_SDK_ALLOW_DOWNLOAD ON CACHE BOOL "Allow C SDK download at configure time")
set(AIC_SDK_USE_STATIC ON CACHE BOOL "Use static linking for AIC SDK (optional)")
set(AIC_SDK_ROOT "" CACHE PATH "Path to preinstalled ai-coustics C SDK (with include/ and lib/)")
set(AIC_SDK_URL "" CACHE STRING "Override C SDK URL (optional)")
set(AIC_SDK_HASH "" CACHE STRING "SHA256 of the C SDK archive (recommended)")

# Path to local aic-sdk-cpp (optional)
set(AIC_SDK_CPP_LOCAL_PATH "" CACHE PATH "Path to local aic-sdk-cpp directory")

# Use local path if provided, otherwise fetch from Git
if(AIC_SDK_CPP_LOCAL_PATH AND EXISTS "${AIC_SDK_CPP_LOCAL_PATH}")
    add_subdirectory("${AIC_SDK_CPP_LOCAL_PATH}" aic_sdk)
else()
    FetchContent_Declare(
        aic_sdk
        GIT_REPOSITORY https://github.com/ai-coustics/aic-sdk-cpp.git
        GIT_TAG        0.6.2
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(aic_sdk)
endif()

add_executable(my_app main.cpp)

# Link against the SDK wrapper target
target_link_libraries(my_app PRIVATE aic-sdk)
