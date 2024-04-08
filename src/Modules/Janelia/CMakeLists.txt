cmake_minimum_required(VERSION 3.21)
project(janelia
        VERSION 0.0.1
        DESCRIPTION "Janelia whisker tracking in Modern C++")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#This is necessary to work on windows. A better way would be to define only the symbols that need to be exported
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_library(janelia SHARED
    janelia.h
    janelia.cpp
    detector_bank.h
    detector_bank.cpp)

target_include_directories(janelia
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    )

    
