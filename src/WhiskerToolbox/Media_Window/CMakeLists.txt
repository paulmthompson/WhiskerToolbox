

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_library(Media_Window SHARED
        Media_Window.hpp
        Media_Window.cpp
)

target_include_directories(Media_Window PUBLIC
                            "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>")

target_link_libraries(Media_Window PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

target_link_libraries(Media_Window PRIVATE DataManager)
target_link_libraries(Media_Window PRIVATE ffmpeg_wrapper::ffmpeg_wrapper)
