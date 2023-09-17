cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#This is necessary to work on windows. A better way would be to define only the symbols that need to be exported
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_library(Media_Window SHARED
    Media_Window.h
    Video_Window.h
    Images_Window.h
    Media_Window.cpp
    Video_Window.cpp
    Images_Window.cpp)


target_include_directories(Media_Window PUBLIC
                            "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

target_link_libraries(Media_Window PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(Media_Window PRIVATE Qt${QT_VERSION_MAJOR}::Multimedia)
target_link_libraries(Media_Window PRIVATE Qt${QT_VERSION_MAJOR}::MultimediaWidgets)
target_link_libraries(Media_Window PRIVATE Qt${QT_VERSION_MAJOR}::OpenGL)
target_link_libraries(Media_Window PRIVATE Qt${QT_VERSION_MAJOR}::UiTools)

target_link_libraries(Media_Window PRIVATE ffmpeg_wrapper::ffmpeg_wrapper)

target_link_libraries(Media_Window PRIVATE DataManager)
