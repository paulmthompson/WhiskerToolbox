
find_package(HighFive CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)

set(QT_ENABLE_VERBOSE_DEPLOYMENT TRUE)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Xml Svg PrintSupport)

qt_standard_project_setup()

add_subdirectory(DataManager)

set(PROJECT_SOURCES
        main.cpp
        whiskertoolbox.qrc
        my_stylesheet.qss

        AnalogViewer/analog_viewer.hpp
        AnalogViewer/analog_viewer.cpp
        AnalogViewer/analog_viewer.ui
        AnalogViewer/DigitalTimeSeriesGraph.cpp
        AnalogViewer/DigitalTimeSeriesGraph.hpp

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

        Media_Widget/Media_Widget.hpp
        Media_Widget/Media_Widget.cpp
        Media_Widget/Media_Widget.ui
        Media_Widget/Media_Widget_Items.hpp
        Media_Widget/Media_Widget_Items.cpp
        Media_Widget/Media_Widget_Items.ui

        TimeScrollBar/TimeScrollBar.hpp
        TimeScrollBar/TimeScrollBar.cpp
        TimeScrollBar/TimeScrollBar.ui

        Image_Processing_Widget/Image_Processing_Widget.hpp
        Image_Processing_Widget/Image_Processing_Widget.cpp
        Image_Processing_Widget/Image_Processing_Widget.ui

        Tongue_Widget/Tongue_Widget.hpp
        Tongue_Widget/Tongue_Widget.cpp
        Tongue_Widget/Tongue_Widget.ui

        Grabcut_Widget/Grabcut_Widget.hpp
        Grabcut_Widget/Grabcut_Widget.cpp
        Grabcut_Widget/GrabCutTool.cpp
        Grabcut_Widget/GrabCutTool.hpp
        Grabcut_Widget/Grabcut_Widget.ui

        Magic_Eraser_Widget/magic_eraser.hpp
        Magic_Eraser_Widget/magic_eraser.cpp
)

if (WIN32)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /fsanitize=address")
endif()

qt_add_executable(WhiskerToolbox
        ${PROJECT_SOURCES}
)

target_link_libraries(WhiskerToolbox PRIVATE Qt6::Widgets Qt6::PrintSupport Qt6::Xml)

target_link_libraries(WhiskerToolbox PRIVATE HighFive)
target_link_libraries(WhiskerToolbox PRIVATE nlohmann_json::nlohmann_json)

target_include_directories(WhiskerToolbox PRIVATE

        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/AnalogViewer>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Covariate_Widget>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/DataManager>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Label_Widget>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Main_Window>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Media_Window>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Whisker_Widget>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Grabcut_Widget>"

        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(WhiskerToolbox PRIVATE Whisker-Analysis)
target_link_libraries(WhiskerToolbox PRIVATE DataManager)

target_link_libraries(WhiskerToolbox PRIVATE opencv_core opencv_imgproc opencv_highgui opencv_photo)

target_link_libraries(WhiskerToolbox PRIVATE JKQTPlotter6::JKQTPlotter6)

target_link_libraries(WhiskerToolbox PRIVATE qt6advanceddocking)


if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(WhiskerToolbox PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(WhiskerToolbox PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(WhiskerToolbox PRIVATE ${MSVC_WARNINGS})
endif()

set_target_properties(WhiskerToolbox PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(${CMAKE_SOURCE_DIR}/packaging/Install.cmake)
