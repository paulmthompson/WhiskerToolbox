
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
