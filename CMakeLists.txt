cmake_minimum_required(VERSION 3.21)

project(WhiskerToolbox VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Multimedia)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS MultimediaWidgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS OpenGL)

#find_package(ffmpeg_wrapper CONFIG REQUIRED PATHS "/home/wanglab/Documents/ffmpeg_wrapper/build/test")
find_package(ffmpeg_wrapper CONFIG REQUIRED PATHS "C:\\Users\\Paul\\Documents\\ffmpeg_wrapper\\build\\test")

include_directories("headers")

set(PROJECT_SOURCES
        src/main.cpp
        src/mainwindow.cpp
        headers/mainwindow.h
        src/mainwindow.ui
        headers/whiskertracker.h
        src/whiskertracker.cpp
        whiskertoolbox.qrc
        headers/janelia.h
        src/janelia.cpp
        my_stylesheet.qss
        headers/detector_bank.h
        src/detector_bank.cpp
        headers/video_window.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(WhiskerToolbox
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET WhiskerToolbox APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(WhiskerToolbox SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(WhiskerToolbox
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(WhiskerToolbox PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(WhiskerToolbox PRIVATE Qt${QT_VERSION_MAJOR}::Multimedia)
target_link_libraries(WhiskerToolbox PRIVATE Qt${QT_VERSION_MAJOR}::MultimediaWidgets)
target_link_libraries(WhiskerToolbox PRIVATE Qt${QT_VERSION_MAJOR}::OpenGL)

target_link_libraries(WhiskerToolbox PRIVATE ffmpeg_wrapper::ffmpeg_wrapper)

set_target_properties(WhiskerToolbox PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(WhiskerToolbox)
endif()
