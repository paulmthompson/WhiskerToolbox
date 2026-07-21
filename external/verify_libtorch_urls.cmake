# @file verify_libtorch_urls.cmake
# @brief Script-mode HTTP probe of libtorch URLs in pytorch_stack_urls.cmake.
#
# Current pins (from cache defaults or -D overrides):
#   cmake -P external/verify_libtorch_urls.cmake
#
# Every (libtorch version, CUDA tag) in the manifest registry:
#   cmake -DNEURALYZER_VERIFY_ALL_LIBTORCH_CONFIGS=ON -P external/verify_libtorch_urls.cmake

cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED NEURALYZER_VERIFY_ALL_LIBTORCH_CONFIGS)
    set(NEURALYZER_VERIFY_ALL_LIBTORCH_CONFIGS OFF)
endif()

get_filename_component(_verify_script_dir "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
include("${_verify_script_dir}/pytorch_stack_urls.cmake")

find_program(CURL_EXECUTABLE curl REQUIRED)

function(_neuralyzer_probe_url url)
    message(STATUS "Probing libtorch URL: ${url}")
    execute_process(
        COMMAND "${CURL_EXECUTABLE}" -sfI -L "${url}"
        RESULT_VARIABLE _curl_result
        OUTPUT_QUIET
        ERROR_QUIET)
    if(_curl_result)
        message(FATAL_ERROR
            "Missing or unreachable libtorch artifact (curl exit ${_curl_result}): ${url}")
    endif()
endfunction()

if(NEURALYZER_VERIFY_ALL_LIBTORCH_CONFIGS)
    foreach(_version IN LISTS NEURALYZER_LIBTORCH_MANIFEST_VERSIONS)
        string(REPLACE "." "_" _version_id "${_version}")
        set(_cuda_tags_var "NEURALYZER_LIBTORCH_MANIFEST_${_version_id}_CUDA_TAGS")
        if(NOT DEFINED ${_cuda_tags_var})
            message(FATAL_ERROR
                "Missing ${_cuda_tags_var} for libtorch ${_version} (listed in MANIFEST_VERSIONS)")
        endif()
        foreach(_cuda_tag IN LISTS ${_cuda_tags_var})
            set(NEURALYZER_LIBTORCH_VERSION "${_version}")
            set(NEURALYZER_LIBTORCH_CUDA_TAG "${_cuda_tag}")
            neuralyzer_resolve_libtorch_manifest()
            message(STATUS "=== libtorch ${_version}, CUDA tag ${_cuda_tag} ===")
            foreach(_url IN LISTS NEURALYZER_LIBTORCH_URLS_FOR_VERIFY)
                _neuralyzer_probe_url("${_url}")
            endforeach()
        endforeach()
    endforeach()
    message(STATUS "All manifest libtorch/CUDA combinations responded OK.")
else()
    neuralyzer_resolve_libtorch_manifest()
    message(STATUS "Verifying libtorch ${NEURALYZER_LIBTORCH_VERSION}, "
                   "CUDA tag ${NEURALYZER_LIBTORCH_CUDA_TAG}")
    foreach(_url IN LISTS NEURALYZER_LIBTORCH_URLS_FOR_VERIFY)
        _neuralyzer_probe_url("${_url}")
    endforeach()
    message(STATUS "Libtorch manifest URLs responded OK.")
endif()
