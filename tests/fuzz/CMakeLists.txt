# Fuzz testing with Google FuzzTest
# Only enabled when ENABLE_FUZZ_TESTING is ON and using Clang

if(NOT ENABLE_FUZZ_TESTING)
    return()
endif()

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "Fuzz testing requires Clang compiler. Skipping fuzz tests.")
    return()
endif()

# Fetch Google FuzzTest
Include(FetchContent)

# CRITICAL: Override abseil's find_package so FuzzTest uses vcpkg's abseil
# This prevents FuzzTest from fetching its own abseil via FetchContent
FetchContent_Declare(
    abseil-cpp
    FIND_PACKAGE_ARGS NAMES absl
#    OVERRIDE_FIND_PACKAGE
)

# get newer googletest to avoid conflicts
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.16.0
)

# Now find abseil from vcpkg - this will be used by FuzzTest
find_package(absl CONFIG REQUIRED)
message(STATUS "Found abseil from vcpkg - FuzzTest will use this version")


# Tell FuzzTest not to build its own RE2 and googletest since we have them or don't need them
set(FUZZTEST_BUILD_TESTING OFF CACHE BOOL "Disable FuzzTest's own tests" FORCE)

# Disable Riegeli dependency in Centipede (part of FuzzTest)
add_compile_definitions(CENTIPEDE_DISABLE_RIEGELI)

# Save and clear CMAKE_CXX_FLAGS which may contain -stdlib=libstdc++ and other flags
# that conflict with sanitizers during abseil compilation
set(SAVED_CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
set(SAVED_CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "")
set(CMAKE_C_FLAGS "")

FetchContent_Declare(
    fuzztest
    GIT_REPOSITORY https://github.com/google/fuzztest.git
    GIT_TAG 2025-02-14
)

# Build FuzzTest and its dependencies WITHOUT inherited flags to avoid conflicts
# We'll add necessary flags only to our fuzz test executables later
FetchContent_MakeAvailable(fuzztest)

# Helper function to link absl_log_instantiations to fuzz test executables
# This ensures Clang 18 gets the explicit template instantiations it needs
function(add_fuzz_test_with_instantiations target_name)
    if(TARGET absl_log_instantiations)
        target_link_libraries(${target_name} PRIVATE absl_log_instantiations)
    endif()
endfunction()


#target_compile_definitions(fuzztest CENTIPEDE_DISABLE_RIEGELI)

# Restore flags for the rest of the build (not that we'll use them much)
set(CMAKE_CXX_FLAGS "${SAVED_CMAKE_CXX_FLAGS}")
set(CMAKE_C_FLAGS "${SAVED_CMAKE_C_FLAGS}")

# Include fuzztest helpers
include(GoogleTest)

# Add subdirectories for different types of fuzz tests
add_subdirectory(unit)
add_subdirectory(integration)

# Create corpus directory structure if it doesn't exist
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/corpus)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/corpus/unit)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/corpus/integration)
