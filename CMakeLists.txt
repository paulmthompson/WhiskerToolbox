#=============================
# Preamble
#=============================
cmake_minimum_required(VERSION 3.24)
project(WhiskerToolbox VERSION 0.1 LANGUAGES CXX)


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

#[[
Manifest mode from VCPKG will make all dependencies specified in vcpkg.json be associated and installed
with this particular project
]]
set(VCPKG_MANIFEST_MODE ON)

#=============================
# Externally Provided Content
#=============================

add_subdirectory(external)

#=============================
# Main Build Targets
#=============================

add_subdirectory(src)