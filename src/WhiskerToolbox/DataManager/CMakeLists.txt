cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#This is necessary to work on windows. A better way would be to define only the symbols that need to be exported
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_library(DataManager SHARED
    DataManager.h
    DataManager.cpp
    DataSeries.h
    DataSeries.cpp
    TimeFrame.h
    TimeFrame.cpp
)

target_include_directories(DataManager PUBLIC
                            "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

target_link_libraries(DataManager PRIVATE ffmpeg_wrapper::ffmpeg_wrapper)
