# @file pytorch_stack.cmake
# @brief LibTorch FetchContent selection and ExecuTorch FetchContent configuration.

include("${CMAKE_CURRENT_LIST_DIR}/pytorch_stack_urls.cmake")
neuralyzer_resolve_libtorch_manifest()

set(NEURALYZER_LIBTORCH_URL "" CACHE STRING
    "Override libtorch zip URL for this configure (empty = use manifest in pytorch_stack_urls.cmake)")

option(NEURALYZER_VERIFY_LIBTORCH_URL_AT_CONFIGURE
    "HTTP-probe the selected libtorch URL before FetchContent (requires curl)" OFF)

# Resolves the manifest URL for the current platform and ENABLE_NEURALYZER_CUDA.
# @param out_url Output variable name for the selected URL string.
function(neuralyzer_resolve_libtorch_url out_url)
    if(NEURALYZER_LIBTORCH_URL)
        set("${out_url}" "${NEURALYZER_LIBTORCH_URL}" PARENT_SCOPE)
        return()
    endif()

    if(LINUX)
        if(ENABLE_NEURALYZER_CUDA)
            set(_selected "${NEURALYZER_LIBTORCH_URL_LINUX_CUDA}")
        else()
            set(_selected "${NEURALYZER_LIBTORCH_URL_LINUX_CPU}")
        endif()
    elseif(APPLE)
        set(_selected "${NEURALYZER_LIBTORCH_URL_MACOS_CPU}")
    elseif(WIN32)
        if(ENABLE_NEURALYZER_CUDA)
            set(_selected "${NEURALYZER_LIBTORCH_URL_WINDOWS_CUDA}")
        else()
            set(_selected "${NEURALYZER_LIBTORCH_URL_WINDOWS_CPU}")
        endif()
    else()
        message(FATAL_ERROR
            "neuralyzer_resolve_libtorch_url: unsupported platform (set LINUX, APPLE, or WIN32)")
    endif()

    set("${out_url}" "${_selected}" PARENT_SCOPE)
endfunction()

function(_neuralyzer_probe_libtorch_url url)
    find_program(_neuralyzer_curl curl)
    if(NOT _neuralyzer_curl)
        message(FATAL_ERROR
            "NEURALYZER_VERIFY_LIBTORCH_URL_AT_CONFIGURE is ON but curl was not found in PATH")
    endif()
    execute_process(
        COMMAND "${_neuralyzer_curl}" -sfI -L "${url}"
        RESULT_VARIABLE _curl_result
        OUTPUT_QUIET
        ERROR_QUIET)
    if(_curl_result)
        message(FATAL_ERROR
            "LibTorch URL probe failed (curl exit ${_curl_result}): ${url}\n"
            "See https://download.pytorch.org/libtorch/${NEURALYZER_LIBTORCH_CUDA_TAG}/")
    endif()
endfunction()

# Declares FetchContent for the platform-appropriate libtorch zip.
function(neuralyzer_declare_libtorch)
    neuralyzer_resolve_libtorch_url(_libtorch_url)

    if(ENABLE_NEURALYZER_CUDA AND (LINUX OR WIN32))
        message(STATUS "Fetching libtorch ${NEURALYZER_LIBTORCH_VERSION}, CUDA "
                       "${NEURALYZER_CUDA_VERSION} (tag ${NEURALYZER_LIBTORCH_CUDA_TAG})")
        message(STATUS "  URL: ${_libtorch_url}")
    else()
        message(STATUS "Fetching libtorch ${NEURALYZER_LIBTORCH_VERSION} (CPU)")
        message(STATUS "  URL: ${_libtorch_url}")
    endif()

    if(NEURALYZER_VERIFY_LIBTORCH_URL_AT_CONFIGURE)
        _neuralyzer_probe_libtorch_url("${_libtorch_url}")
    endif()

    FetchContent_Declare(
        Torch
        URL "${_libtorch_url}")
endfunction()

# Declares FetchContent for ExecuTorch and sets build options for libtorch-only integration.
function(neuralyzer_declare_executorch)
    message(STATUS "ExecuTorch support enabled (tag ${NEURALYZER_EXECUTORCH_GIT_TAG})")

    FetchContent_Declare(
        executorch
        GIT_REPOSITORY https://github.com/pytorch/executorch.git
        GIT_TAG ${NEURALYZER_EXECUTORCH_GIT_TAG}
        SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_deps/executorch # https://github.com/pytorch/executorch/issues/6475
    )

    # Do NOT set EXECUTORCH_BUILD_PRESET_FILE to linux.cmake — it chains into
    # llm.cmake which enables EXECUTORCH_BUILD_KERNELS_OPTIMIZED and other features
    # that require a pip-installed torch (Python `import torch`). We use libtorch
    # (C++ only) via FetchContent, so we must disable those code paths.
    set(EXECUTORCH_BUILD_PORTABLE_OPS OFF)
    set(EXECUTORCH_BUILD_EXECUTOR_RUNNER OFF)

    # Enable ExecuTorch extensions needed by the ModelExecution wrapper (Phase 3).
    # extension_module depends on extension_data_loader and extension_flat_tensor.
    set(EXECUTORCH_BUILD_EXTENSION_DATA_LOADER ON)
    set(EXECUTORCH_BUILD_EXTENSION_MODULE ON)
    set(EXECUTORCH_BUILD_EXTENSION_TENSOR ON)
    set(EXECUTORCH_BUILD_EXTENSION_FLAT_TENSOR ON)
    set(EXECUTORCH_BUILD_EXTENSION_NAMED_DATA_MAP ON)
endfunction()
