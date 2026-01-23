cmake_minimum_required(VERSION 3.14)
project(aic-sdk-cpp VERSION 0.14.0 LANGUAGES C CXX)

option(AIC_SDK_ALLOW_DOWNLOAD "Allow C SDK download at configure time" OFF)
option(AIC_SDK_USE_STATIC "Link against static aic C SDK" ON)

# -------- SDK Platform Configuration --------
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/checksum.txt" AIC_SDK_CHECKSUMS)

foreach(line ${AIC_SDK_CHECKSUMS})
    # Parse line: <hash>  <filename>
    string(REGEX MATCH "^([a-f0-9]+)[ \t]+(.+)$" _ "${line}")
    set(hash "${CMAKE_MATCH_1}")
    set(filename "${CMAKE_MATCH_2}")

    # Parse filename: aic-sdk-<triplet>-<version>.<ext>
    if("${filename}" MATCHES "aic-sdk-(.+)-([0-9]+\\.[0-9]+\\.[0-9]+)\\.(tar\\.gz|zip)")
        set(triplet "${CMAKE_MATCH_1}")
        set(version "${CMAKE_MATCH_2}")
        set(ext "${CMAKE_MATCH_3}")

        set(AIC_SDK_HASH_${triplet} "${hash}")
        set(AIC_SDK_ARCHIVE_EXT_${triplet} "${ext}")
        set(AIC_SDK_VERSION "${version}")
    endif()
endforeach()

if(NOT AIC_SDK_VERSION)
    message(FATAL_ERROR "Could not determine AIC SDK version from checksum.txt")
endif()

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
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
            set(t "aarch64-pc-windows-msvc")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
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

if(AIC_SDK_ROOT)
    set(AIC_C_INCLUDE_DIR "${AIC_SDK_ROOT}/include")
    set(AIC_C_LIBDIR      "${AIC_SDK_ROOT}/lib")
else()
    if(NOT AIC_SDK_ALLOW_DOWNLOAD)
        message(FATAL_ERROR "AIC_SDK_ROOT not set and downloads disabled (AIC_SDK_ALLOW_DOWNLOAD=OFF).")
    endif()

    include(FetchContent)
    set(AIC_SDK_URL
        "https://github.com/ai-coustics/aic-sdk-c/releases/download/${AIC_SDK_VERSION}/aic-sdk-${AIC_TRIPLET}-${AIC_SDK_VERSION}.${AIC_ARCHIVE_EXT}")

    set(AIC_SDK_HASH "${AIC_SDK_HASH_${AIC_TRIPLET}}")

    FetchContent_Declare(aic_sdk_c
        URL "${AIC_SDK_URL}"
        URL_HASH "SHA256=${AIC_SDK_HASH}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

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

# -------- C++ wrapper --------
add_library(aic-sdk src/aic.cpp)

target_link_libraries(aic-sdk PUBLIC aic_c)
target_include_directories(aic-sdk PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(aic-sdk PUBLIC cxx_std_11)

# Add required system frameworks on macOS
if(APPLE)
    target_link_libraries(aic-sdk PUBLIC
        "-framework CoreFoundation"
    )
endif()

# Add required system libraries on Windows
if(WIN32)
    # Provide import lib for ProcessPrng which is exported by bcryptprimitives.dll
    # The Windows SDK does not ship an import lib for it. We synthesize one
    # from a .def file and make aic-sdk depend on it, so the file exists
    # when consumers link.
    set(BCRYPTPRIMITIVES_STUB_LIB ${CMAKE_CURRENT_BINARY_DIR}/bcryptprimitives_extra.lib)

    # Determine machine architecture for lib.exe
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
        set(LIB_MACHINE "ARM64")
    else()
        set(LIB_MACHINE "X64")
    endif()

    add_custom_command(
        OUTPUT ${BCRYPTPRIMITIVES_STUB_LIB}
        COMMAND lib /DEF:${CMAKE_CURRENT_SOURCE_DIR}/bcryptprimitives_extra.def /OUT:${BCRYPTPRIMITIVES_STUB_LIB} /MACHINE:${LIB_MACHINE}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/bcryptprimitives_extra.def
        VERBATIM
    )
    add_custom_target(bcryptprimitives_stub ALL DEPENDS ${BCRYPTPRIMITIVES_STUB_LIB})
    add_dependencies(aic-sdk bcryptprimitives_stub)

    # Link standard Windows SDK libs required by the C SDK
    # - ws2_32: networking
    # - bcrypt: crypto primitives
    # - Synchronization: WaitOnAddress/WakeByAddress*
    # - ntdll: Nt* and Rtl* used by deps
    # - bcryptprimitives_extra.lib: import for ProcessPrng
    target_link_libraries(aic-sdk PUBLIC
        ntdll
        ws2_32
        bcrypt
        Synchronization
        ${BCRYPTPRIMITIVES_STUB_LIB}
    )
endif()
