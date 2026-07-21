# @file pytorch_stack_urls.cmake
# @brief LibTorch / CUDA version pins and explicit download URLs.
#
# Set NEURALYZER_LIBTORCH_VERSION and NEURALYZER_LIBTORCH_CUDA_TAG independently
# (CUDA tag is used when ENABLE_NEURALYZER_CUDA is ON). URLs are full literals from
# https://download.pytorch.org/libtorch/ — paste new links when adding support.
#
# After editing: cmake -P external/verify_libtorch_urls.cmake

# -----------------------------------------------------------------------------
# Version pins (configure-time cache)
# -----------------------------------------------------------------------------
set(NEURALYZER_LIBTORCH_VERSION "2.9.0" CACHE STRING
    "LibTorch release to download (supported values in neuralyzer_resolve_libtorch_manifest)")

set(NEURALYZER_LIBTORCH_CUDA_TAG "cu126" CACHE STRING
    "PyTorch libtorch CUDA folder/tag (e.g. cu126, cu128, cu130, cu121). Used when ENABLE_NEURALYZER_CUDA is ON.")

set(NEURALYZER_EXECUTORCH_GIT_TAG "v1.0.1" CACHE STRING
    "ExecuTorch git tag (pair with libtorch per https://github.com/pytorch/executorch/releases)")

# ExecuTorch ↔ libtorch reference:
#   v1.1.0 → 2.11.0   v1.0.1 → 2.9.0   v0.7.0 → 2.8.0

# Libtorch versions in neuralyzer_resolve_libtorch_manifest; verify-all probes every CUDA tag listed per version.
set(NEURALYZER_LIBTORCH_MANIFEST_VERSIONS "2.9.0" "2.5.1")
set(NEURALYZER_LIBTORCH_MANIFEST_2_9_0_CUDA_TAGS "cu126" "cu128" "cu130")
set(NEURALYZER_LIBTORCH_MANIFEST_2_5_1_CUDA_TAGS "cu121")

# -----------------------------------------------------------------------------
# Populates NEURALYZER_CUDA_VERSION and NEURALYZER_LIBTORCH_URL_* for the current
# NEURALYZER_LIBTORCH_VERSION and NEURALYZER_LIBTORCH_CUDA_TAG.
# -----------------------------------------------------------------------------
macro(neuralyzer_resolve_libtorch_manifest)
    unset(NEURALYZER_CUDA_VERSION)
    unset(NEURALYZER_LIBTORCH_URL_LINUX_CPU)
    unset(NEURALYZER_LIBTORCH_URL_LINUX_CUDA)
    unset(NEURALYZER_LIBTORCH_URL_MACOS_CPU)
    unset(NEURALYZER_LIBTORCH_URL_WINDOWS_CPU)
    unset(NEURALYZER_LIBTORCH_URL_WINDOWS_CUDA)

    # ----- LibTorch 2.9.0 ------------------------------------------------------
    if(NEURALYZER_LIBTORCH_VERSION VERSION_EQUAL "2.9.0")

        set(NEURALYZER_LIBTORCH_URL_LINUX_CPU
            "https://download.pytorch.org/libtorch/cpu/libtorch-shared-with-deps-2.9.0%2Bcpu.zip")
        set(NEURALYZER_LIBTORCH_URL_MACOS_CPU
            "https://download.pytorch.org/libtorch/cpu/libtorch-macos-arm64-2.9.0.zip")
        set(NEURALYZER_LIBTORCH_URL_WINDOWS_CPU
            "https://download.pytorch.org/libtorch/cpu/libtorch-win-shared-with-deps-2.9.0%2Bcpu.zip")

        if(NEURALYZER_LIBTORCH_CUDA_TAG STREQUAL "cu126")
            set(NEURALYZER_CUDA_VERSION "12.6")
            set(NEURALYZER_LIBTORCH_URL_LINUX_CUDA
                "https://download.pytorch.org/libtorch/cu126/libtorch-shared-with-deps-2.9.0%2Bcu126.zip")
            set(NEURALYZER_LIBTORCH_URL_WINDOWS_CUDA
                "https://download.pytorch.org/libtorch/cu126/libtorch-win-shared-with-deps-2.9.0%2Bcu126.zip")
        elseif(NEURALYZER_LIBTORCH_CUDA_TAG STREQUAL "cu128")
            set(NEURALYZER_CUDA_VERSION "12.8")
            set(NEURALYZER_LIBTORCH_URL_LINUX_CUDA
                "https://download.pytorch.org/libtorch/cu128/libtorch-shared-with-deps-2.9.0%2Bcu128.zip")
            set(NEURALYZER_LIBTORCH_URL_WINDOWS_CUDA
                "https://download.pytorch.org/libtorch/cu128/libtorch-win-shared-with-deps-2.9.0%2Bcu128.zip")
        elseif(NEURALYZER_LIBTORCH_CUDA_TAG STREQUAL "cu130")
            set(NEURALYZER_CUDA_VERSION "13.0")
            set(NEURALYZER_LIBTORCH_URL_LINUX_CUDA
                "https://download.pytorch.org/libtorch/cu130/libtorch-shared-with-deps-2.9.0%2Bcu130.zip")
            set(NEURALYZER_LIBTORCH_URL_WINDOWS_CUDA
                "https://download.pytorch.org/libtorch/cu130/libtorch-win-shared-with-deps-2.9.0%2Bcu130.zip")
        else()
            message(FATAL_ERROR
                "LibTorch 2.9.0: unsupported NEURALYZER_LIBTORCH_CUDA_TAG=\"${NEURALYZER_LIBTORCH_CUDA_TAG}\". "
                "Add a cu* block in pytorch_stack_urls.cmake.")
        endif()

    # ----- LibTorch 2.5.1 (Linux cxx11-abi zips) -----------------------------
    elseif(NEURALYZER_LIBTORCH_VERSION VERSION_EQUAL "2.5.1")

        set(NEURALYZER_LIBTORCH_URL_LINUX_CPU
            "https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-2.5.1%2Bcpu.zip")
        set(NEURALYZER_LIBTORCH_URL_MACOS_CPU
            "https://download.pytorch.org/libtorch/cpu/libtorch-macos-arm64-2.5.1.zip")
        set(NEURALYZER_LIBTORCH_URL_WINDOWS_CPU
            "https://download.pytorch.org/libtorch/cpu/libtorch-win-shared-with-deps-2.5.1%2Bcpu.zip")

        if(NEURALYZER_LIBTORCH_CUDA_TAG STREQUAL "cu121")
            set(NEURALYZER_CUDA_VERSION "12.1")
            set(NEURALYZER_LIBTORCH_URL_LINUX_CUDA
                "https://download.pytorch.org/libtorch/cu121/libtorch-cxx11-abi-shared-with-deps-2.5.1%2Bcu121.zip")
            set(NEURALYZER_LIBTORCH_URL_WINDOWS_CUDA
                "https://download.pytorch.org/libtorch/cu121/libtorch-win-shared-with-deps-2.5.1%2Bcu121.zip")
        else()
            message(FATAL_ERROR
                "LibTorch 2.5.1: unsupported NEURALYZER_LIBTORCH_CUDA_TAG=\"${NEURALYZER_LIBTORCH_CUDA_TAG}\". "
                "Add a cu* block in pytorch_stack_urls.cmake.")
        endif()

    else()
        message(FATAL_ERROR
            "Unsupported NEURALYZER_LIBTORCH_VERSION=\"${NEURALYZER_LIBTORCH_VERSION}\". "
            "Add a version block in pytorch_stack_urls.cmake.")
    endif()

    set(NEURALYZER_LIBTORCH_URLS_FOR_VERIFY
        "${NEURALYZER_LIBTORCH_URL_LINUX_CPU}"
        "${NEURALYZER_LIBTORCH_URL_LINUX_CUDA}"
        "${NEURALYZER_LIBTORCH_URL_MACOS_CPU}"
        "${NEURALYZER_LIBTORCH_URL_WINDOWS_CPU}"
        "${NEURALYZER_LIBTORCH_URL_WINDOWS_CUDA}")
endmacro()
