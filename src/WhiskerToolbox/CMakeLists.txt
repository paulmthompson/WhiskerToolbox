
find_package(HighFive CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Multimedia)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS MultimediaWidgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS UiTools REQUIRED)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS OpenGL)

set(PROJECT_SOURCES
        main.cpp
        whiskertoolbox.qrc
        my_stylesheet.qss

        Main_Window/mainwindow.cpp
        Main_Window/mainwindow.h
        Main_Window/mainwindow.ui

        Covariate_Widget/Covariate_Widget.h
        Covariate_Widget/covariate_widget.ui
        Covariate_Widget/Covariate_Config.h
        Covariate_Widget/Covariate_Config.ui

        Whisker_Widget/Whisker_Widget.h
        Whisker_Widget/Whisker_Widget.ui
        Whisker_Widget/whiskertracker.h
        Whisker_Widget/Whisker_Widget.cpp
        Whisker_Widget/whiskertracker.cpp

        Label_Widget/Label_Widget.h
        Label_Widget/Label_Widget.ui
        Label_Widget/Label_Widget.cpp
        Label_Widget/label_maker.h
        Label_Widget/label_maker.cpp
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
target_link_libraries(WhiskerToolbox PRIVATE Qt${QT_VERSION_MAJOR}::UiTools)

target_link_libraries(WhiskerToolbox PRIVATE ffmpeg_wrapper::ffmpeg_wrapper)

target_link_libraries(WhiskerToolbox PRIVATE HighFive)
target_link_libraries(WhiskerToolbox PRIVATE nlohmann_json::nlohmann_json)

#target_link_libraries(WhiskerToolbox PRIVATE PNG::PNG)

add_subdirectory(DataManager)
add_subdirectory(Media_Window)

add_subdirectory(AnalogViewer)

target_include_directories(WhiskerToolbox PRIVATE
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/Modules/Janelia>"

        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Main_Window>"

        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/DataManager>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Media_Window>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Whisker_Widget>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Label_Widget>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Covariate_Widget>"
        
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/AnalogViewer>"

        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(WhiskerToolbox PRIVATE janelia)
target_link_libraries(WhiskerToolbox PRIVATE DataManager)
target_link_libraries(WhiskerToolbox PRIVATE Media_Window)
target_link_libraries(WhiskerToolbox PRIVATE Analog_Viewer)

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
