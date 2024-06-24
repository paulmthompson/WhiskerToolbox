
find_package(HighFive CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

set(PROJECT_SOURCES
        main.cpp
        whiskertoolbox.qrc
        my_stylesheet.qss

        AnalogViewer/analog_viewer.hpp
        AnalogViewer/analog_viewer.cpp
        AnalogViewer/analog_viewer.ui

        Main_Window/mainwindow.cpp
        Main_Window/mainwindow.hpp
        Main_Window/mainwindow.ui

        Covariate_Widget/Covariate_Widget.h
        Covariate_Widget/covariate_widget.ui
        Covariate_Widget/Covariate_Config.h
        Covariate_Widget/Covariate_Config.ui

        Whisker_Widget/Whisker_Widget.hpp
        Whisker_Widget/Whisker_Widget.ui
        Whisker_Widget/Whisker_Widget.cpp
        Whisker_Widget/janelia_config.ui
        Whisker_Widget/janelia_config.hpp
        Whisker_Widget/janelia_config.cpp
        Whisker_Widget/contact_widget.ui
        Whisker_Widget/contact_widget.hpp
        Whisker_Widget/contact_widget.cpp

        Label_Widget/Label_Widget.hpp
        Label_Widget/Label_Widget.ui
        Label_Widget/Label_Widget.cpp
        Label_Widget/label_maker.hpp
        Label_Widget/label_maker.cpp

        Media_Window/Media_Window.hpp
        Media_Window/Media_Window.cpp

        TimeScrollBar/TimeScrollBar.hpp
        TimeScrollBar/TimeScrollBar.cpp
        TimeScrollBar/TimeScrollBar.ui

        Image_Processing_Widget/Image_Processing_Widget.hpp
        Image_Processing_Widget/Image_Processing_Widget.cpp
        Image_Processing_Widget/Image_Processing_Widget.ui

        Tongue_Widget/Tongue_Widget.hpp
        Tongue_Widget/Tongue_Widget.cpp
        Tongue_Widget/Tongue_Widget.ui
)


qt_add_executable(WhiskerToolbox
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
)

target_link_libraries(WhiskerToolbox PRIVATE Qt6::Widgets)

target_link_libraries(WhiskerToolbox PRIVATE ffmpeg_wrapper::ffmpeg_wrapper)

target_link_libraries(WhiskerToolbox PRIVATE HighFive)
target_link_libraries(WhiskerToolbox PRIVATE nlohmann_json::nlohmann_json)

add_subdirectory(DataManager)

target_include_directories(WhiskerToolbox PRIVATE

        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/AnalogViewer>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Covariate_Widget>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/DataManager>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Label_Widget>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Main_Window>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Media_Window>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Whisker_Widget>"

        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(WhiskerToolbox PRIVATE Whisker-Analysis)
target_link_libraries(WhiskerToolbox PRIVATE DataManager)

target_link_libraries(WhiskerToolbox PRIVATE opencv_core opencv_imgproc opencv_imgcodecs)

set_target_properties(WhiskerToolbox PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
)

qt_finalize_executable(WhiskerToolbox)


if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_INSTALL_BINDIR})
endif()

if(APPLE)
    # Assume targeting macOS to keep this example simple
    set_target_properties(WhiskerToolbox PROPERTIES
    INSTALL_RPATH @executable_path/../Frameworks
    )
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(WhiskerToolbox PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(WhiskerToolbox PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(WhiskerToolbox PRIVATE ${MSVC_WARNINGS})
endif()

install(TARGETS WhiskerToolbox DataManager
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_app_script(
    TARGET WhiskerToolbox
    OUTPUT_SCRIPT deployApp
)

install(SCRIPT "${deployApp}")

include(CPack)
