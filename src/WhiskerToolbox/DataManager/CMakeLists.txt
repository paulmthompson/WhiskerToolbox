
add_library(DataManager SHARED
        DataManager.h
        DataManager.cpp
        DataSeries.h
        DataSeries.cpp
        TimeFrame.h
        TimeFrame.cpp

        Image_Data.cpp
        Image_Data.h
        Video_Data.cpp
        Video_Data.hpp
        Media_Data.h
        Media_Data.cpp
)

target_include_directories(DataManager PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

target_link_libraries(DataManager PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(DataManager PRIVATE Qt${QT_VERSION_MAJOR}::Multimedia)

target_link_libraries(DataManager PRIVATE ffmpeg_wrapper::ffmpeg_wrapper)
