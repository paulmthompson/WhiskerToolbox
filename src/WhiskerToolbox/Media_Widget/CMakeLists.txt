
find_package(OpenCV CONFIG REQUIRED)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)


set(MEDIA_WIDGET_SOURCES
    Media_Window/Media_Window.hpp
    Media_Window/Media_Window.cpp

    Media_Widget.hpp
    Media_Widget.cpp
    Media_Widget.ui
    MediaWidgetState.hpp
    MediaWidgetState.cpp
    MediaWidgetStateData.hpp
    DisplayOptions/DisplayOptions.hpp
    DisplayOptions/CoordinateTypes.hpp

        MediaProcessing_Widget/MediaProcessing_Widget.cpp
        MediaProcessing_Widget/MediaProcessing_Widget.hpp
        MediaProcessing_Widget/MediaProcessing_Widget.ui
        MediaProcessing_Widget/ProcessingOptions/ColormapWidget.cpp
        MediaProcessing_Widget/ProcessingOptions/ColormapWidget.hpp
        MediaProcessing_Widget/ProcessingOptions/ColormapWidget.ui
        MediaProcessing_Widget/ProcessingOptions/ContrastWidget.cpp
        MediaProcessing_Widget/ProcessingOptions/ContrastWidget.hpp
        MediaProcessing_Widget/ProcessingOptions/ContrastWidget.ui
        MediaProcessing_Widget/ProcessingOptions/GammaWidget.cpp
        MediaProcessing_Widget/ProcessingOptions/GammaWidget.hpp
        MediaProcessing_Widget/ProcessingOptions/GammaWidget.ui
        MediaProcessing_Widget/ProcessingOptions/SharpenWidget.cpp
        MediaProcessing_Widget/ProcessingOptions/SharpenWidget.hpp
        MediaProcessing_Widget/ProcessingOptions/SharpenWidget.ui
        MediaProcessing_Widget/ProcessingOptions/BilateralWidget.cpp
        MediaProcessing_Widget/ProcessingOptions/BilateralWidget.hpp
        MediaProcessing_Widget/ProcessingOptions/BilateralWidget.ui
        MediaProcessing_Widget/ProcessingOptions/ClaheWidget.cpp
        MediaProcessing_Widget/ProcessingOptions/ClaheWidget.hpp
        MediaProcessing_Widget/ProcessingOptions/ClaheWidget.ui
        MediaProcessing_Widget/ProcessingOptions/MedianWidget.cpp
        MediaProcessing_Widget/ProcessingOptions/MedianWidget.hpp
        MediaProcessing_Widget/ProcessingOptions/MedianWidget.ui
        MediaProcessing_Widget/ProcessingOptions/MagicEraserWidget.cpp
        MediaProcessing_Widget/ProcessingOptions/MagicEraserWidget.hpp
        MediaProcessing_Widget/ProcessingOptions/MagicEraserWidget.ui

        MediaMask_Widget/MediaMask_Widget.hpp
        MediaMask_Widget/MediaMask_Widget.cpp
        MediaMask_Widget/MediaMask_Widget.ui
        MediaMask_Widget/SelectionWidgets/MaskNoneSelectionWidget.cpp
        MediaMask_Widget/SelectionWidgets/MaskNoneSelectionWidget.hpp
        MediaMask_Widget/SelectionWidgets/MaskNoneSelectionWidget.ui
        MediaMask_Widget/SelectionWidgets/MaskBrushSelectionWidget.cpp
        MediaMask_Widget/SelectionWidgets/MaskBrushSelectionWidget.hpp
        MediaMask_Widget/SelectionWidgets/MaskBrushSelectionWidget.ui
        MediaMask_Widget/MaskDilationWidget/MaskDilationWidget.cpp
        MediaMask_Widget/MaskDilationWidget/MaskDilationWidget.hpp
        MediaMask_Widget/MaskDilationWidget/MaskDilationWidget.ui

        MediaPoint_Widget/MediaPoint_Widget.hpp
        MediaPoint_Widget/MediaPoint_Widget.cpp
        MediaPoint_Widget/MediaPoint_Widget.ui

        MediaTensor_Widget/MediaTensor_Widget.hpp
        MediaTensor_Widget/MediaTensor_Widget.cpp
        MediaTensor_Widget/MediaTensor_Widget.ui

        MediaLine_Widget/MediaLine_Widget.hpp
        MediaLine_Widget/MediaLine_Widget.cpp
        MediaLine_Widget/MediaLine_Widget.ui
        MediaLine_Widget/SelectionWidgets/LineNoneSelectionWidget.cpp
        MediaLine_Widget/SelectionWidgets/LineNoneSelectionWidget.hpp
        MediaLine_Widget/SelectionWidgets/LineNoneSelectionWidget.ui
        MediaLine_Widget/SelectionWidgets/LineAddSelectionWidget.cpp
        MediaLine_Widget/SelectionWidgets/LineAddSelectionWidget.hpp
        MediaLine_Widget/SelectionWidgets/LineAddSelectionWidget.ui
        MediaLine_Widget/SelectionWidgets/LineEraseSelectionWidget.cpp
        MediaLine_Widget/SelectionWidgets/LineEraseSelectionWidget.hpp
        MediaLine_Widget/SelectionWidgets/LineEraseSelectionWidget.ui
        MediaLine_Widget/SelectionWidgets/LineSelectSelectionWidget.cpp
        MediaLine_Widget/SelectionWidgets/LineSelectSelectionWidget.hpp
        MediaLine_Widget/SelectionWidgets/LineSelectSelectionWidget.ui
        MediaLine_Widget/SelectionWidgets/LineDrawAllFramesSelectionWidget.cpp
        MediaLine_Widget/SelectionWidgets/LineDrawAllFramesSelectionWidget.hpp
        MediaLine_Widget/SelectionWidgets/LineDrawAllFramesSelectionWidget.ui

        MediaInterval_Widget/MediaInterval_Widget.hpp
        MediaInterval_Widget/MediaInterval_Widget.cpp
        MediaInterval_Widget/MediaInterval_Widget.ui
        MediaInterval_Widget/StyleWidgets/BoxIntervalStyle_Widget.hpp
        MediaInterval_Widget/StyleWidgets/BoxIntervalStyle_Widget.cpp
        MediaInterval_Widget/StyleWidgets/BoxIntervalStyle_Widget.ui
        MediaInterval_Widget/StyleWidgets/BorderIntervalStyle_Widget.hpp
        MediaInterval_Widget/StyleWidgets/BorderIntervalStyle_Widget.cpp
        MediaInterval_Widget/StyleWidgets/BorderIntervalStyle_Widget.ui

        MediaText_Widget/MediaText_Widget.hpp
        MediaText_Widget/MediaText_Widget.cpp
        MediaText_Widget/MediaText_Widget.ui

        MouseBrush_Widget/MouseBrush_Widget.hpp
        MouseBrush_Widget/MouseBrush_Widget.cpp
        MouseBrush_Widget/MouseBrush_Widget.ui

)

add_library(Media_Widget STATIC ${MEDIA_WIDGET_SOURCES})

target_link_libraries(Media_Widget PRIVATE Qt6::Widgets)
target_link_libraries(Media_Widget PRIVATE Feature_Table_Widget)
target_link_libraries(Media_Widget PRIVATE EditorState)
target_link_libraries(Media_Widget PUBLIC NEURALYZER_GEOMETRY)
target_link_libraries(Media_Widget PRIVATE TimeScrollBar)
target_link_libraries(Media_Widget PRIVATE DataManager)
target_link_libraries(Media_Widget PRIVATE ColorPicker_Widget)
target_link_libraries(Media_Widget PRIVATE Collapsible_Widget)
target_link_libraries(Media_Widget PRIVATE GroupManagementWidget)
target_link_libraries(Media_Widget PRIVATE GroupContextMenu)
target_link_libraries(Media_Widget PRIVATE opencv_core opencv_imgproc opencv_highgui opencv_photo)

target_include_directories(Media_Widget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)


if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(Media_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(Media_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(Media_Widget PRIVATE ${MSVC_WARNINGS})
endif()

