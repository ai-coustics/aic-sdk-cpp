cmake_minimum_required(VERSION 3.14)
project(aic-sdk-cpp VERSION 0.1.0 LANGUAGES C CXX)

option(AIC_SDK_USE_STATIC "Link against static aic C SDK" ON)
option(AIC_SDK_ALLOW_DOWNLOAD "Allow C SDK download at configure time" OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# -------- SDK Version Configuration --------
set(AIC_SDK_VERSION "0.6.2")

# -------- SDK Platform Configuration --------
# Map of supported platforms with their triplets, archive extensions, and hashes
set(AIC_SDK_PLATFORMS
    # triplet|archive_ext|hash
    "aarch64-apple-darwin|tar.gz|7f2a3f1705113a8e0eb4e01cd24afbc959b84451b33fc9aaaea7475e71971fc6"
    "aarch64-unknown-linux-gnu|tar.gz|25b065eb82dfd21934c244e0e1307bfa6b49af25c3e68986e8e1a0e6f151332d"
    "x86_64-apple-darwin|tar.gz|5b86b2359427adb56000a88308c75cc9b636890abd5211120a5735ea1e75c696"
    "x86_64-pc-windows-msvc|zip|c662138cd4d997fec0b1e0fd7312c1693d976a3fe15d4236409d1b645b879281"
    "x86_64-unknown-linux-gnu|tar.gz|bc410a5aea213bcbf3a3dd9e2192fbd308ff2e15312479f559138814352cc15a"
)

# Parse platform configuration into individual variables
foreach(platform_config ${AIC_SDK_PLATFORMS})
    string(REPLACE "|" ";" platform_parts "${platform_config}")
    list(GET platform_parts 0 triplet)
    list(GET platform_parts 1 archive_ext)
    list(GET platform_parts 2 hash)

    set(AIC_SDK_HASH_${triplet} "${hash}" CACHE STRING "SHA256 for ${triplet} SDK")
    set(AIC_SDK_ARCHIVE_EXT_${triplet} "${archive_ext}")
endforeach()

# -------- Platform / filename helpers --------
function(get_aic_sdk_triplet out)
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            set(t "aarch64-apple-darwin")
        else()
            set(t "x86_64-apple-darwin")
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            set(t "aarch64-unknown-linux-gnu")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
            set(t "x86_64-unknown-linux-gnu")
        else()
            message(FATAL_ERROR "Unsupported Linux arch: ${CMAKE_SYSTEM_PROCESSOR}")
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
            set(t "x86_64-pc-windows-msvc")
        else()
            message(FATAL_ERROR "Unsupported Windows arch: ${CMAKE_SYSTEM_PROCESSOR}")
        endif()
    else()
        message(FATAL_ERROR "Unsupported OS: ${CMAKE_SYSTEM_NAME}")
    endif()
    set(${out} "${t}" PARENT_SCOPE)
endfunction()

get_aic_sdk_triplet(AIC_TRIPLET)

# Get archive extension for current platform
set(AIC_ARCHIVE_EXT "${AIC_SDK_ARCHIVE_EXT_${AIC_TRIPLET}}")

# -------- Locate C SDK (prefer user-provided) --------
set(AIC_SDK_ROOT "" CACHE PATH "Path to a preinstalled ai-coustics C SDK (with include/ and lib/)")
set(AIC_SDK_URL  "" CACHE STRING "Override C SDK URL (optional)")
set(AIC_SDK_HASH "" CACHE STRING "SHA256 of the C SDK archive (recommended)")

if(AIC_SDK_ROOT)
    set(AIC_C_INCLUDE_DIR "${AIC_SDK_ROOT}/include")
    set(AIC_C_LIBDIR      "${AIC_SDK_ROOT}/lib")
else()
    if(NOT AIC_SDK_ALLOW_DOWNLOAD)
        message(FATAL_ERROR "AIC_SDK_ROOT not set and downloads disabled (AIC_SDK_ALLOW_DOWNLOAD=OFF).")
    endif()

    include(FetchContent)
    if(NOT AIC_SDK_URL)
        set(AIC_SDK_URL
            "https://github.com/ai-coustics/aic-sdk-c/releases/download/${AIC_SDK_VERSION}/aic-sdk-${AIC_TRIPLET}-${AIC_SDK_VERSION}.${AIC_ARCHIVE_EXT}")
    endif()

    # Auto-select hash based on triplet if not explicitly provided
    if(NOT AIC_SDK_HASH)
        set(AIC_SDK_HASH "${AIC_SDK_HASH_${AIC_TRIPLET}}")

    endif()

    if(NOT AIC_SDK_HASH)
        message(WARNING "No URL_HASH provided for ${AIC_SDK_URL}. For public releases, set AIC_SDK_HASH_${AIC_TRIPLET}.")
        FetchContent_Declare(aic_sdk_c
            URL "${AIC_SDK_URL}"
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    else()
        FetchContent_Declare(aic_sdk_c
            URL "${AIC_SDK_URL}"
            URL_HASH "SHA256=${AIC_SDK_HASH}"
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    endif()

    FetchContent_MakeAvailable(aic_sdk_c)

    set(AIC_C_INCLUDE_DIR "${aic_sdk_c_SOURCE_DIR}/include")
    set(AIC_C_LIBDIR      "${aic_sdk_c_SOURCE_DIR}/lib")
endif()

# -------- Decide library filenames --------
if(WIN32)
    if(AIC_SDK_USE_STATIC)
        set(AIC_LIB_FILE "${AIC_C_LIBDIR}/aic.lib")
        set(AIC_LIB_KIND STATIC)
    else()
        set(AIC_LIB_FILE "${AIC_C_LIBDIR}/aic.dll")
        set(AIC_IMPLIB   "${AIC_C_LIBDIR}/aic.lib")
        set(AIC_LIB_KIND SHARED)
    endif()
elseif(APPLE)
    if(AIC_SDK_USE_STATIC)
        set(AIC_LIB_FILE "${AIC_C_LIBDIR}/libaic.a")
        set(AIC_LIB_KIND STATIC)
    else()
        set(AIC_LIB_FILE "${AIC_C_LIBDIR}/libaic.dylib")
        set(AIC_LIB_KIND SHARED)
    endif()
else() # Linux
    if(AIC_SDK_USE_STATIC)
        set(AIC_LIB_FILE "${AIC_C_LIBDIR}/libaic.a")
        set(AIC_LIB_KIND STATIC)
    else()
        set(AIC_LIB_FILE "${AIC_C_LIBDIR}/libaic.so")
        set(AIC_LIB_KIND SHARED)
    endif()
endif()

# -------- Imported C SDK target (build tree) --------
add_library(aic_c ${AIC_LIB_KIND} IMPORTED GLOBAL)

# We only ship RELEASE binaries; map all consumer configs to Release.
set_target_properties(aic_c PROPERTIES
    IMPORTED_CONFIGURATIONS RELEASE
    IMPORTED_LOCATION_RELEASE "${AIC_LIB_FILE}"
    INTERFACE_INCLUDE_DIRECTORIES "${AIC_C_INCLUDE_DIR}"
    MAP_IMPORTED_CONFIG_DEBUG          Release
    MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
    MAP_IMPORTED_CONFIG_MINSIZEREL     Release
)
if(WIN32 AND NOT AIC_SDK_USE_STATIC AND DEFINED AIC_IMPLIB)
    set_target_properties(aic_c PROPERTIES
        IMPORTED_IMPLIB_RELEASE "${AIC_IMPLIB}"
    )
endif()

# -------- Header-only C++ wrapper (single public name) --------
add_library(aic-sdk INTERFACE)

target_link_libraries(aic-sdk INTERFACE aic_c)
target_include_directories(aic-sdk INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(aic-sdk INTERFACE cxx_std_17)

# Add required system frameworks on macOS
if(APPLE)
    target_link_libraries(aic-sdk INTERFACE
        "-framework CoreFoundation"
    )
endif()

# Add required system libraries on Windows
if(WIN32)
    target_link_libraries(aic-sdk INTERFACE
        ntdll
    )
endif()

# -------- Summary --------
message(STATUS "=== aic-sdk-cpp ===")
message(STATUS "  Version:      ${PROJECT_VERSION}")
message(STATUS "  SDK Version:  ${AIC_SDK_VERSION}")
message(STATUS "  Triplet:      ${AIC_TRIPLET}")
message(STATUS "  Static:       ${AIC_SDK_USE_STATIC}")
message(STATUS "  SDK root:     ${AIC_SDK_ROOT}")
message(STATUS "  SDK URL:      ${AIC_SDK_URL}")
message(STATUS "  Using lib:    ${AIC_LIB_FILE}")
if(WIN32 AND NOT AIC_SDK_USE_STATIC)
    message(STATUS "  Using implib: ${AIC_IMPLIB}")
endif()
