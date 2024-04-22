
add_library(DataManager SHARED
        DataManager.h
        DataManager.cpp
        DataSeries.h
        DataSeries.cpp
        TimeFrame.h
        TimeFrame.cpp

        Media/Image_Data.cpp
        Media/Image_Data.h
        Media/Video_Data.cpp
        Media/Video_Data.hpp
        Media/Media_Data.h
        Media/Media_Data.cpp
)

target_include_directories(DataManager PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

target_link_libraries(DataManager PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(DataManager PRIVATE Qt${QT_VERSION_MAJOR}::Multimedia)

target_link_libraries(DataManager PRIVATE ffmpeg_wrapper::ffmpeg_wrapper)
