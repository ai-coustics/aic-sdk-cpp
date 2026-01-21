cmake_minimum_required(VERSION 3.14)
project(MyProject LANGUAGES C CXX)

# Export compile commands for vscode, vim, zed etc.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Require C++11 for the example app
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# All ai-coustics CMake configuration settings
set(AIC_SDK_ALLOW_DOWNLOAD ON CACHE BOOL "Allow download ai-coustics C SDK")
set(AIC_SDK_USE_STATIC ON CACHE BOOL "Use static linking for AIC SDK")
set(AIC_SDK_ROOT "" CACHE PATH "Path to local ai-coustics C SDK (always skips download if set)")

# Path to local C++ wrapper
set(AIC_SDK_CPP_PATH "../" CACHE PATH "Path to local aic-sdk-cpp directory")

if(AIC_SDK_CPP_PATH AND EXISTS "${AIC_SDK_CPP_PATH}")
    # Use the local C++ wrapper
    add_subdirectory("${AIC_SDK_CPP_PATH}" aic_sdk)
else()
    include(FetchContent)
    # Download the C++ wrapper
    FetchContent_Declare(
        aic_sdk
        GIT_REPOSITORY https://github.com/ai-coustics/aic-sdk-cpp.git
        GIT_TAG        0.13.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(aic_sdk)
endif()

add_executable(my_app main.cpp)

# Link against the SDK
target_link_libraries(my_app PRIVATE aic-sdk)

if(APPLE)
    target_link_libraries(my_app PRIVATE "-framework Security" "-framework CoreFoundation")
endif()

if(WIN32)
    target_link_libraries(my_app PRIVATE crypt32)
endif()
