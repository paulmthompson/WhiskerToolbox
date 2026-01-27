
find_package(OpenCV CONFIG REQUIRED)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)


set(MEDIA_WIDGET_SOURCES

    MediaWidgetRegistration.hpp
    MediaWidgetRegistration.cpp

    Core/MediaWidgetState.hpp
    Core/MediaWidgetState.cpp
    Core/MediaWidgetStateData.hpp

    Rendering/Media_Window/Media_Window.hpp
    Rendering/Media_Window/Media_Window.cpp

    UI/Media_Widget.hpp
    UI/Media_Widget.cpp
    UI/Media_Widget.ui
    UI/MediaPropertiesWidget.hpp
    UI/MediaPropertiesWidget.cpp
    UI/MediaPropertiesWidget.ui

    UI/SubWidgets/MediaProcessing_Widget/MediaProcessing_Widget.cpp
    UI/SubWidgets/MediaProcessing_Widget/MediaProcessing_Widget.hpp
    UI/SubWidgets/MediaProcessing_Widget/MediaProcessing_Widget.ui
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/ColormapWidget.cpp
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/ColormapWidget.hpp
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/ColormapWidget.ui
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/ContrastWidget.cpp
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/ContrastWidget.hpp
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/ContrastWidget.ui
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/GammaWidget.cpp
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/GammaWidget.hpp
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/GammaWidget.ui
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/SharpenWidget.cpp
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/SharpenWidget.hpp
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/SharpenWidget.ui
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/BilateralWidget.cpp
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/BilateralWidget.hpp
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/BilateralWidget.ui
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/ClaheWidget.cpp
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/ClaheWidget.hpp
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/ClaheWidget.ui
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/MedianWidget.cpp
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/MedianWidget.hpp
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/MedianWidget.ui
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/MagicEraserWidget.cpp
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/MagicEraserWidget.hpp
    UI/SubWidgets/MediaProcessing_Widget/ProcessingOptions/MagicEraserWidget.ui

    UI/SubWidgets/MediaMask_Widget/MediaMask_Widget.hpp
    UI/SubWidgets/MediaMask_Widget/MediaMask_Widget.cpp
    UI/SubWidgets/MediaMask_Widget/MediaMask_Widget.ui
    UI/SubWidgets/MediaMask_Widget/SelectionWidgets/MaskNoneSelectionWidget.cpp
    UI/SubWidgets/MediaMask_Widget/SelectionWidgets/MaskNoneSelectionWidget.hpp
    UI/SubWidgets/MediaMask_Widget/SelectionWidgets/MaskNoneSelectionWidget.ui
    UI/SubWidgets/MediaMask_Widget/SelectionWidgets/MaskBrushSelectionWidget.cpp
    UI/SubWidgets/MediaMask_Widget/SelectionWidgets/MaskBrushSelectionWidget.hpp
    UI/SubWidgets/MediaMask_Widget/SelectionWidgets/MaskBrushSelectionWidget.ui
    UI/SubWidgets/MediaMask_Widget/MaskDilationWidget/MaskDilationWidget.cpp
    UI/SubWidgets/MediaMask_Widget/MaskDilationWidget/MaskDilationWidget.hpp
    UI/SubWidgets/MediaMask_Widget/MaskDilationWidget/MaskDilationWidget.ui
    UI/SubWidgets/MediaPoint_Widget/MediaPoint_Widget.hpp
    UI/SubWidgets/MediaPoint_Widget/MediaPoint_Widget.cpp
    UI/SubWidgets/MediaPoint_Widget/MediaPoint_Widget.ui

    UI/SubWidgets/MediaTensor_Widget/MediaTensor_Widget.hpp
    UI/SubWidgets/MediaTensor_Widget/MediaTensor_Widget.cpp
    UI/SubWidgets/MediaTensor_Widget/MediaTensor_Widget.ui
    UI/SubWidgets/MediaLine_Widget/MediaLine_Widget.hpp
    UI/SubWidgets/MediaLine_Widget/MediaLine_Widget.cpp
    UI/SubWidgets/MediaLine_Widget/MediaLine_Widget.ui
    UI/SubWidgets/MediaLine_Widget/SelectionWidgets/LineNoneSelectionWidget.cpp
    UI/SubWidgets/MediaLine_Widget/SelectionWidgets/LineNoneSelectionWidget.hpp
    UI/SubWidgets/MediaLine_Widget/SelectionWidgets/LineNoneSelectionWidget.ui
    UI/SubWidgets/MediaLine_Widget/SelectionWidgets/LineAddSelectionWidget.cpp
    UI/SubWidgets/MediaLine_Widget/SelectionWidgets/LineAddSelectionWidget.hpp
    UI/SubWidgets/MediaLine_Widget/SelectionWidgets/LineAddSelectionWidget.ui
    UI/SubWidgets/MediaLine_Widget/SelectionWidgets/LineEraseSelectionWidget.cpp
    UI/SubWidgets/MediaLine_Widget/SelectionWidgets/LineEraseSelectionWidget.hpp
    UI/SubWidgets/MediaLine_Widget/SelectionWidgets/LineEraseSelectionWidget.ui
    UI/SubWidgets/MediaLine_Widget/SelectionWidgets/LineSelectSelectionWidget.cpp
    UI/SubWidgets/MediaLine_Widget/SelectionWidgets/LineSelectSelectionWidget.hpp
    UI/SubWidgets/MediaLine_Widget/SelectionWidgets/LineSelectSelectionWidget.ui
    UI/SubWidgets/MediaLine_Widget/SelectionWidgets/LineDrawAllFramesSelectionWidget.cpp
    UI/SubWidgets/MediaLine_Widget/SelectionWidgets/LineDrawAllFramesSelectionWidget.hpp
    UI/SubWidgets/MediaLine_Widget/SelectionWidgets/LineDrawAllFramesSelectionWidget.ui

    UI/SubWidgets/MediaInterval_Widget/MediaInterval_Widget.hpp
    UI/SubWidgets/MediaInterval_Widget/MediaInterval_Widget.cpp
    UI/SubWidgets/MediaInterval_Widget/MediaInterval_Widget.ui
    UI/SubWidgets/MediaInterval_Widget/StyleWidgets/BoxIntervalStyle_Widget.hpp
    UI/SubWidgets/MediaInterval_Widget/StyleWidgets/BoxIntervalStyle_Widget.cpp
    UI/SubWidgets/MediaInterval_Widget/StyleWidgets/BoxIntervalStyle_Widget.ui
    UI/SubWidgets/MediaInterval_Widget/StyleWidgets/BorderIntervalStyle_Widget.hpp
    UI/SubWidgets/MediaInterval_Widget/StyleWidgets/BorderIntervalStyle_Widget.cpp
    UI/SubWidgets/MediaInterval_Widget/StyleWidgets/BorderIntervalStyle_Widget.ui

    UI/SubWidgets/MediaText_Widget/MediaText_Widget.hpp
    UI/SubWidgets/MediaText_Widget/MediaText_Widget.cpp
    UI/SubWidgets/MediaText_Widget/MediaText_Widget.ui

    UI/SubWidgets/MouseBrush_Widget/MouseBrush_Widget.hpp
    UI/SubWidgets/MouseBrush_Widget/MouseBrush_Widget.cpp
    UI/SubWidgets/MouseBrush_Widget/MouseBrush_Widget.ui

    DisplayOptionsRegistry.hpp
    DisplayOptionsRegistry.cpp
    DisplayOptions/DisplayOptions.hpp
    DisplayOptions/CoordinateTypes.hpp




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

