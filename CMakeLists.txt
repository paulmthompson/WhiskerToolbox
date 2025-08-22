#=============================
# Preamble
#=============================
cmake_minimum_required(VERSION 3.25)

# CapnProto data format support - MUST be before project() call
option(ENABLE_CAPNPROTO "Enable CapnProto serialization support for data loading/saving" OFF)

# HDF5 data format support - MUST be before project() call
option(ENABLE_HDF5 "Enable HDF5 support for data loading/saving" OFF)

# FFmpeg video support - MUST be before project() call
option(ENABLE_FFMPEG "Enable FFmpeg support for video loading/saving" OFF)

# Tensor backend selection - MUST be before project() call
option(TENSOR_BACKEND_LIBTORCH "Use LibTorch as tensor backend (enables deep learning)" OFF)

option(ENABLE_OPENCV "Enable OpenCV support for image mask loading/saving" OFF)

option(ENABLE_UI "Enable UI" OFF)

# Configure vcpkg features based on CMake options - MUST be before project() call
set(VCPKG_MANIFEST_FEATURES "")
if(ENABLE_CAPNPROTO)
    list(APPEND VCPKG_MANIFEST_FEATURES "capnproto")
    message(STATUS "CapnProto support: ENABLED")
else()
    message(STATUS "CapnProto support: DISABLED")
endif()

if(ENABLE_HDF5)
    list(APPEND VCPKG_MANIFEST_FEATURES "hdf5")
    message(STATUS "HDF5 support: ENABLED")
else()
    message(STATUS "HDF5 support: DISABLED")
endif()

if(ENABLE_FFMPEG)
    list(APPEND VCPKG_MANIFEST_FEATURES "ffmpeg")
    message(STATUS "FFmpeg support: ENABLED")
else()
    message(STATUS "FFmpeg support: DISABLED")
endif()

if(ENABLE_OPENCV)
   # list(APPEND VCPKG_MANIFEST_FEATURES "opencv") // Not ready to do this yet
    message(STATUS "OpenCV support: ENABLED")
else()
    message(STATUS "OpenCV support: DISABLED")
endif()

if(ENABLE_UI)
    list(APPEND VCPKG_MANIFEST_FEATURES "ui")
    message(STATUS "UI support: ENABLED")
else()
    message(STATUS "UI support: DISABLED")
endif()

if(TENSOR_BACKEND_LIBTORCH)
    message(STATUS "Tensor backend: LibTorch (deep learning enabled)")
else()
    message(STATUS "Tensor backend: Armadillo (lightweight, no deep learning)")
endif()

message(STATUS "VCPKG_MANIFEST_FEATURES: ${VCPKG_MANIFEST_FEATURES}")

project(BehaviorToolbox VERSION 0.3.5 LANGUAGES CXX)

include(CTest)

#=============================
# Project Wide Setup
#=============================

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#[[
Best practice for looking for cmake modules cmake folder in root of the project
Currently, this project does not have any cmake modules
From Professional CMake Chapter 11 Modules
]]
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(enable_sanitizers)
include(CompilerWarnings)
include(set_rpath)
include(iwyu)
include(DataManagerUtils)

#only have coverage option on linux
if (LINUX)
    option(CODE_COVERAGE "Enable coverage reporting" OFF)
    if(CODE_COVERAGE)
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
    endif()
endif()

#[[
Manifest mode from VCPKG will make all dependencies specified in vcpkg.json be associated and installed
with this particular project
]]
set(VCPKG_MANIFEST_MODE ON)
set(VCPKG_INSTALL_OPTIONS "--allow-unsupported")

#This is necessary to work on windows. A better way would be to define only the symbols that need to be exported
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_SUPPORT_WINDOWS_EXPORT_ALL_SYMBOLS 1)

#Fix header linting on vscode
#e.g. add "compileCommands": "${workspaceFolder}/out/build/Clang/Release/compile_commands.json" to c_cpp_properties.json for Mac config
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
#=============================
# Externally Provided Content
#=============================

add_subdirectory(external)

#=============================
# Main Build Targets
#=============================

add_subdirectory(src)

#=============================
# Testing
#=============================

#[[
BUILD_TESTING is a cache variable that defaults to true which was defined by the CTest module
If we wish to run testing, we should add the tests subdirectory.
]]
if (BUILD_TESTING)
    add_subdirectory(tests)
endif()
