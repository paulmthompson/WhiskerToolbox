# @file verify_libtorch_urls.cmake
# @brief Script-mode HTTP probe of libtorch URLs in pytorch_stack_urls.cmake.
#
# Current pins (from cache defaults or -D overrides):
#   cmake -P external/verify_libtorch_urls.cmake
#
# Every entry in NEURALYZER_LIBTORCH_VERIFY_CONFIGS:
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
    list(LENGTH NEURALYZER_LIBTORCH_VERIFY_VERSIONS _verify_count)
    list(LENGTH NEURALYZER_LIBTORCH_VERIFY_CUDA_TAGS _cuda_count)
    if(NOT _verify_count EQUAL _cuda_count)
        message(FATAL_ERROR
            "NEURALYZER_LIBTORCH_VERIFY_VERSIONS and VERIFY_CUDA_TAGS length mismatch")
    endif()
    math(EXPR _last_index "${_verify_count} - 1")
    foreach(_index RANGE 0 ${_last_index})
        list(GET NEURALYZER_LIBTORCH_VERIFY_VERSIONS ${_index} _version)
        list(GET NEURALYZER_LIBTORCH_VERIFY_CUDA_TAGS ${_index} _cuda_tag)
        set(NEURALYZER_LIBTORCH_VERSION "${_version}")
        set(NEURALYZER_LIBTORCH_CUDA_TAG "${_cuda_tag}")
        neuralyzer_resolve_libtorch_manifest()
        message(STATUS "=== libtorch ${_version}, CUDA tag ${_cuda_tag} ===")
        foreach(_url IN LISTS NEURALYZER_LIBTORCH_URLS_FOR_VERIFY)
            _neuralyzer_probe_url("${_url}")
        endforeach()
    endforeach()
    message(STATUS "All NEURALYZER_LIBTORCH_VERIFY_VERSIONS entries responded OK.")
else()
    neuralyzer_resolve_libtorch_manifest()
    message(STATUS "Verifying libtorch ${NEURALYZER_LIBTORCH_VERSION}, "
                   "CUDA tag ${NEURALYZER_LIBTORCH_CUDA_TAG}")
    foreach(_url IN LISTS NEURALYZER_LIBTORCH_URLS_FOR_VERIFY)
        _neuralyzer_probe_url("${_url}")
    endforeach()
    message(STATUS "Libtorch manifest URLs responded OK.")
endif()
