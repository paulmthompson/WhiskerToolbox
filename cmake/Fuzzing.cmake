# Fuzzing.cmake
# CMake module for configuring fuzzing support with libFuzzer

option(ENABLE_FUZZING "Enable fuzzing tests with libFuzzer" OFF)

if(ENABLE_FUZZING)
    # Fuzzing requires Clang compiler
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(WARNING "Fuzzing is only supported with Clang compiler. Disabling fuzzing.")
        set(ENABLE_FUZZING OFF CACHE BOOL "Enable fuzzing tests with libFuzzer" FORCE)
        return()
    endif()

    message(STATUS "Fuzzing support: ENABLED")
    
    # Check if libFuzzer is available by trying to compile a simple fuzzer
    # CheckCXXCompilerFlag doesn't work well for -fsanitize=fuzzer because it requires special handling
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=fuzzer")
    check_cxx_source_compiles("
        extern \"C\" int LLVMFuzzerTestOneInput(const unsigned char *data, unsigned long size) { 
            return 0; 
        }
    " COMPILER_SUPPORTS_LIBFUZZER)
    set(CMAKE_REQUIRED_FLAGS "")
    
    if(NOT COMPILER_SUPPORTS_LIBFUZZER)
        message(WARNING "Compiler does not support -fsanitize=fuzzer. Disabling fuzzing.")
        set(ENABLE_FUZZING OFF CACHE BOOL "Enable fuzzing tests with libFuzzer" FORCE)
        return()
    endif()
    
    # Set fuzzing compiler flags
    set(FUZZING_COMPILE_FLAGS -fsanitize=fuzzer,address -g -O1)
    set(FUZZING_LINK_FLAGS -fsanitize=fuzzer,address)
    
    message(STATUS "libFuzzer compile flags: ${FUZZING_COMPILE_FLAGS}")
    message(STATUS "libFuzzer link flags: ${FUZZING_LINK_FLAGS}")
    
    # Function to add a fuzz target
    # Usage: add_fuzz_target(target_name source_file LINK_LIBRARIES lib1 lib2 ...)
    function(add_fuzz_target TARGET_NAME SOURCE_FILE)
        set(options "")
        set(oneValueArgs "")
        set(multiValueArgs LINK_LIBRARIES INCLUDE_DIRECTORIES)
        cmake_parse_arguments(FUZZ_TARGET "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
        
        # Create the fuzz target executable
        add_executable(${TARGET_NAME} ${SOURCE_FILE})
        
        # Apply fuzzing flags
        target_compile_options(${TARGET_NAME} PRIVATE ${FUZZING_COMPILE_FLAGS})
        target_link_options(${TARGET_NAME} PRIVATE ${FUZZING_LINK_FLAGS})
        
        # Link libraries if specified
        if(FUZZ_TARGET_LINK_LIBRARIES)
            target_link_libraries(${TARGET_NAME} PRIVATE ${FUZZ_TARGET_LINK_LIBRARIES})
        endif()
        
        # Add include directories if specified
        if(FUZZ_TARGET_INCLUDE_DIRECTORIES)
            target_include_directories(${TARGET_NAME} PRIVATE ${FUZZ_TARGET_INCLUDE_DIRECTORIES})
        endif()
        
        # Set output directory
        set_target_properties(${TARGET_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}
        )
        
        # Add a short CTest integration (smoke test only)
        # For thorough fuzzing, run the target directly
        add_test(
            NAME ${TARGET_NAME}_smoke
            COMMAND ${TARGET_NAME} -max_total_time=10 -max_len=10240
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
        set_tests_properties(${TARGET_NAME}_smoke PROPERTIES
            LABELS "fuzz"
            TIMEOUT 20
        )
        
        message(STATUS "Added fuzz target: ${TARGET_NAME}")
    endfunction()
    
else()
    message(STATUS "Fuzzing support: DISABLED")
    
    # Provide a no-op function when fuzzing is disabled
    function(add_fuzz_target TARGET_NAME SOURCE_FILE)
        # Do nothing
    endfunction()
endif()
