
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include(FetchContent)
set(BUILD_TESTING OFF)

# Conditionally fetch ffmpeg_wrapper based on ENABLE_FFMPEG flag
if(ENABLE_FFMPEG)
    FetchContent_Declare(
      ffmpeg_wrapper
      GIT_REPOSITORY https://github.com/paulmthompson/ffmpeg_wrapper.git
      #GIT_TAG 807db81584dbf72d34db3659ac01f286bcf7fe53
      GIT_TAG 694f346e5eeaca6c464bb88aa30f43b306c63047
    )
endif()

#add_subdirectory(ffmpeg_wrapper)

FetchContent_Declare(
  Whisker-Analysis
  GIT_REPOSITORY https://github.com/paulmthompson/Whisker-Analysis.git
  GIT_TAG 364fe0c411f7fca785da9b809f22571693eeb839
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
set(FETCHCONTENT_TARGETS "Whisker-Analysis" "Torch" "npy" "iir")

if(ENABLE_FFMPEG)
    list(APPEND FETCHCONTENT_TARGETS "ffmpeg_wrapper")
endif()

if(ENABLE_UI)
    list(APPEND FETCHCONTENT_TARGETS "qt6advanceddocking")
endif()

FetchContent_MakeAvailable(${FETCHCONTENT_TARGETS})

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

set(npy_SOURCE_DIR ${npy_SOURCE_DIR} PARENT_SCOPE)
set(iir_SOURCE_DIR ${iir_SOURCE_DIR} PARENT_SCOPE)
