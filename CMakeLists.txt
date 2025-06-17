#=============================
# Preamble
#=============================
cmake_minimum_required(VERSION 3.25)
project(BehaviorToolbox VERSION 0.3.1 LANGUAGES CXX)

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
