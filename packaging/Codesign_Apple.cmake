
# Find the application bundle's path dynamically
# Note: Adjust the search path according to your project's output structure
file(GLOB_RECURSE APP_BUNDLE_PATHS "${CMAKE_BINARY_DIR}/_CPack_Packages/*/*/WhiskerToolbox.app")
list(GET APP_BUNDLE_PATHS 0 APP_BUNDLE_PATH)

if(NOT APP_BUNDLE_PATH)
    message(FATAL_ERROR "WhiskerToolbox.app not found in CPack package directory.")
else()
    # Proceed with code signing
    execute_process(
            COMMAND codesign --force --deep --verbose --sign "Eric Certificate" "${APP_BUNDLE_PATH}"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE output
            ERROR_VARIABLE error
    )
    if(NOT ${result} EQUAL 0)
        message(FATAL_ERROR "Failed to codesign WhiskerToolbox.app: ${error}")
    else()
        message(STATUS "Successfully codesigned WhiskerToolbox.app at ${APP_BUNDLE_PATH}")
    endif()
endif()