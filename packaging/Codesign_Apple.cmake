
execute_process(
        COMMAND codesign --force --deep --verbose --sign "Eric Certificate" "${CMAKE_INSTALL_PREFIX}/WhiskerToolbox.app"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
)
if(NOT ${result} EQUAL 0)
    message(FATAL_ERROR "Failed to codesign WhiskerToolbox.app: ${error}")
endif()
message(STATUS "Successfully codesigned WhiskerToolbox.app")