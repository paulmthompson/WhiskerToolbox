
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include(FetchContent)
set(BUILD_TESTING OFF)

# Conditionally fetch ffmpeg_wrapper based on ENABLE_FFMPEG flag
if(ENABLE_FFMPEG)
    FetchContent_Declare(
      ffmpeg_wrapper
      GIT_REPOSITORY https://github.com/paulmthompson/ffmpeg_wrapper.git
      GIT_TAG 6ac8ba7760b718c882faa969eccecbf35e507ece
    )
endif()


if (LINUX)
    find_package(absl REQUIRED)
    find_package(Protobuf REQUIRED)
    
    FetchContent_Declare(
        ortools
        GIT_REPOSITORY https://github.com/google/or-tools.git
        GIT_TAG        stable
)
    set(BUILD_SHARED_LIBS OFF) # disables dynamic(shared) library building
    set(BUILD_DEPS OFF)

    set(BUILD_re2 ON)
    set(BUILD_Protobuf OFF)
    set(BUILD_absl OFF)
    
    set(USE_SCIP OFF)
    set(BUILD_soplex OFF)
    set(BUILD_SCIP OFF)
    set(USE_COINOR OFF)
    set(USE_HIGHS OFF)
    set(USE_PDLP OFF)
    set(USE_GLPK OFF)
    set(USE_CPLEX OFF)


    set(BUILD_SAMPLES OFF)
    set(BUILD_EXAMPLES OFF)
    set(BUILD_TESTING OFF)

    set(BUILD_FLATZINC OFF)
    # Keep MATH_OPT enabled as other components depend on it
    # set(BUILD_MATH_OPT OFF)
    
    # Disable all OR-Tools binaries/executables to avoid linking issues
    set(BUILD_CXX_SAMPLES OFF)
    set(BUILD_CXX_EXAMPLES OFF)
elseif (APPLE)
    find_package(absl REQUIRED)
    find_package(Protobuf REQUIRED)
    
    FetchContent_Declare(
        ortools
        GIT_REPOSITORY https://github.com/google/or-tools.git
        GIT_TAG        stable
)
    set(BUILD_SHARED_LIBS OFF) # disables dynamic(shared) library building
    set(BUILD_DEPS OFF)

    set(BUILD_re2 ON)
    set(BUILD_Protobuf OFF)
    set(BUILD_absl OFF)
    
    set(USE_SCIP OFF)
    set(BUILD_soplex OFF)
    set(BUILD_SCIP OFF)
    set(USE_COINOR OFF)
    set(USE_HIGHS OFF)
    set(USE_PDLP OFF)
    set(USE_GLPK OFF)
    set(USE_CPLEX OFF)


    set(BUILD_SAMPLES OFF)
    set(BUILD_EXAMPLES OFF)
    set(BUILD_TESTING OFF)

    set(BUILD_FLATZINC OFF)
    
    # Disable all OR-Tools binaries/executables to avoid linking issues
    set(BUILD_CXX_SAMPLES OFF)
    set(BUILD_CXX_EXAMPLES OFF)
elseif (WIN32)

  find_package(absl REQUIRED)
  find_package(Protobuf REQUIRED)
  find_package(Boost COMPONENTS serialization multiprecision)

  FetchContent_Declare(
      ortools
      GIT_REPOSITORY https://github.com/google/or-tools.git
      GIT_TAG        stable
)
  set(BUILD_SHARED_LIBS OFF) # disables dynamic(shared) library building
  set(BUILD_DEPS OFF)


  set(BUILD_re2 ON)
  set(BUILD_Protobuf OFF)
  set(BUILD_absl OFF)

  set(BUILD_BZip2 ON)

  set(USE_SCIP OFF)
  set(BUILD_soplex OFF)
  set(BUILD_SCIP OFF)
  set(USE_COINOR OFF)
  set(USE_HIGHS OFF)
  set(USE_PDLP OFF)
  set(USE_GLPK OFF)
  set(USE_CPLEX OFF)

  set(BUILD_FLATZINC OFF)
endif()

FetchContent_Declare(
  Whisker-Analysis
  GIT_REPOSITORY https://github.com/paulmthompson/Whisker-Analysis.git
  GIT_TAG bc1d06271a6ddb27b7f939c4a83d0b5f503d8212
  CMAKE_ARGS -DBUILD_WHISKERTRACKER=OFF -BUILD_TESTING=OFF

)

#add_subdirectory(Whisker-Analysis)

if (ENABLE_UI)
    FetchContent_Declare(
    qt6advanceddocking
    GIT_REPOSITORY https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System
    GIT_TAG 4.3.1
    CMAKE_ARGS -DBUILD_EXAMPLES=OFF
    )
endif()

FetchContent_Declare(
  npy
  GIT_REPOSITORY https://github.com/llohse/libnpy.git
  GIT_TAG v1.0.1
)

FetchContent_Declare(
    iir 
    GIT_REPOSITORY https://github.com/berndporr/iir1/
    GIT_TAG 1.9.5
)

FetchContent_Declare(
    pf
    GIT_REPOSITORY https://github.com/paulmthompson/pf
    GIT_TAG 9d017ac42c5662c5764d553fc7f8300def24ca78
)

set(CUDA OFF)

if (LINUX)
    if (CUDA)
        message(STATUS "Fetching libtorch with CUDA")
        FetchContent_Declare(
            Torch
            URL https://download.pytorch.org/libtorch/cu121/libtorch-cxx11-abi-shared-with-deps-2.5.1%2Bcu121.zip
        )
    else()
        FetchContent_Declare(
            Torch
            URL https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-2.5.1%2Bcpu.zip
        )
    endif()
elseif (APPLE)
    FetchContent_Declare(
        Torch
        URL https://download.pytorch.org/libtorch/cpu/libtorch-macos-arm64-2.5.1.zip
    )
elseif (WIN32)
    if (CUDA)
        FetchContent_Declare(
            Torch
            URL https://download.pytorch.org/libtorch/cu121/libtorch-win-shared-with-deps-2.5.1%2Bcu121.zip
        )
    else()
        FetchContent_Declare(
            Torch
            URL https://download.pytorch.org/libtorch/cpu/libtorch-win-shared-with-deps-2.5.1%2Bcpu.zip
        )
    endif()
endif()

#We should use make available
set(FETCHCONTENT_TARGETS "Whisker-Analysis" "Torch" "npy" "iir" "ortools" "pf")

if(ENABLE_FFMPEG)
    list(APPEND FETCHCONTENT_TARGETS "ffmpeg_wrapper")
endif()

if(ENABLE_UI)
    list(APPEND FETCHCONTENT_TARGETS "qt6advanceddocking")
endif()

# Create explicit Abseil log template instantiations for Clang 18 BEFORE building OR-Tools
if(LINUX AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_library(absl_log_instantiations STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/absl_log_instantiations.cpp
    )
    target_link_libraries(absl_log_instantiations PUBLIC absl::log_internal_message)
    set_target_properties(absl_log_instantiations PROPERTIES 
        POSITION_INDEPENDENT_CODE ON
    )
    
    # Make Abseil's log_internal_message library depend on our instantiations
    # This ensures all OR-Tools binaries get the instantiations
    if(TARGET absl::log_internal_message)
        target_link_libraries(absl::log_internal_message INTERFACE absl_log_instantiations)
    endif()
endif()

# Force OR-Tools to build in Release mode to avoid stack protection issues with abseil
set(ortools_BUILD_TYPE_SAVED ${CMAKE_BUILD_TYPE})
set(CMAKE_BUILD_TYPE Release)
FetchContent_MakeAvailable(ortools)
set(CMAKE_BUILD_TYPE ${ortools_BUILD_TYPE_SAVED})

# Make available the rest of the targets
list(REMOVE_ITEM FETCHCONTENT_TARGETS "ortools")
if(FETCHCONTENT_TARGETS)
    FetchContent_MakeAvailable(${FETCHCONTENT_TARGETS})
endif()

# Conditionally set ffmpeg_wrapper properties if it was built
if(ENABLE_FFMPEG)
    set_target_properties(ffmpeg_wrapper PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()
set_target_properties(Whisker-Analysis PROPERTIES EXCLUDE_FROM_ALL TRUE)

if (ENABLE_UI)
    set_target_properties(qt6advanceddocking PROPERTIES
            EXCLUDE_FROM_ALL TRUE
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
            LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${torch_SOURCE_DIR}/share/cmake/Torch" PARENT_SCOPE)

#I need to add the lib folder for ortools to be used when we find_package with ortools


#set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${ortools_SOURCE_DIR}/lib/cmake/ortools" PARENT_SCOPE)

set(npy_SOURCE_DIR ${npy_SOURCE_DIR} PARENT_SCOPE)
set(iir_SOURCE_DIR ${iir_SOURCE_DIR} PARENT_SCOPE)
set(ortools_SOURCE_DIR ${ortools_SOURCE_DIR} PARENT_SCOPE)
set(pf_SOURCE_DIR ${pf_SOURCE_DIR} PARENT_SCOPE)
