#=============================
# Preamble
#=============================
cmake_minimum_required(VERSION 3.25)
project(BehaviorToolbox VERSION 0.1 LANGUAGES CXX)

include(CTest)

#=============================
# Project Wide Setup
#=============================

set(CMAKE_INCLUDE_CURRENT_DIR ON)

#[[
Best practice for looking for cmake modules cmake folder in root of the project
Currently, this project does not have any cmake modules
From Professional CMake Chapter 11 Modules
]]
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /fsanitize=address")
include(enable_sanitizers)
include(CompilerWarnings)
include(set_rpath)
include(iwyu)

#[[
Manifest mode from VCPKG will make all dependencies specified in vcpkg.json be associated and installed
with this particular project
]]
set(VCPKG_MANIFEST_MODE ON)
set(VCPKG_INSTALL_OPTIONS "--allow-unsupported")

#This is necessary to work on windows. A better way would be to define only the symbols that need to be exported
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_SUPPORT_WINDOWS_EXPORT_ALL_SYMBOLS 1)

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
