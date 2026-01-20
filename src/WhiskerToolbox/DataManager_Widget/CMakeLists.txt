find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(DATA_MANAGER_WIDGET_SOURCES
        DataManager_Widget.hpp
        DataManager_Widget.cpp
        DataManager_Widget.ui
        DataManagerWidgetState.hpp
        DataManagerWidgetState.cpp

        Mask_Widget/Mask_Widget.hpp
        Mask_Widget/Mask_Widget.cpp
        Mask_Widget/Mask_Widget.ui
        Mask_Widget/MaskTableModel.cpp
        Mask_Widget/MaskTableModel.hpp

        Point_Widget/Point_Widget.hpp
        Point_Widget/Point_Widget.cpp
        Point_Widget/Point_Widget.ui
        Point_Widget/PointTableModel.cpp
        Point_Widget/PointTableModel.hpp

        Line_Widget/Line_Widget.hpp
        Line_Widget/Line_Widget.cpp
        Line_Widget/Line_Widget.ui
        Line_Widget/LineTableModel.cpp
        Line_Widget/LineTableModel.hpp

        Image_Widget/Image_Widget.hpp
        Image_Widget/Image_Widget.cpp
        Image_Widget/Image_Widget.ui
        Image_Widget/ImageTableModel.hpp
        Image_Widget/ImageTableModel.cpp

        AnalogTimeSeries_Widget/AnalogTimeSeries_Widget.hpp
        AnalogTimeSeries_Widget/AnalogTimeSeries_Widget.cpp
        AnalogTimeSeries_Widget/AnalogTimeSeries_Widget.ui

        DigitalIntervalSeries_Widget/DigitalIntervalSeries_Widget.hpp
        DigitalIntervalSeries_Widget/DigitalIntervalSeries_Widget.cpp
        DigitalIntervalSeries_Widget/DigitalIntervalSeries_Widget.ui
        DigitalIntervalSeries_Widget/IntervalTableModel.hpp

        DigitalEventSeries_Widget/DigitalEventSeries_Widget.hpp
        DigitalEventSeries_Widget/DigitalEventSeries_Widget.cpp
        DigitalEventSeries_Widget/DigitalEventSeries_Widget.ui
        DigitalEventSeries_Widget/EventTableModel.hpp

        Tensor_Widget/Tensor_Widget.hpp
        Tensor_Widget/Tensor_Widget.cpp
        Tensor_Widget/Tensor_Widget.ui
        Tensor_Widget/TensorTableModel.hpp

        OutputDirectoryWidget/OutputDirectoryWidget.hpp
        OutputDirectoryWidget/OutputDirectoryWidget.ui
        OutputDirectoryWidget/OutputDirectoryWidget.cpp

        NewDataWidget/NewDataWidget.hpp
        NewDataWidget/NewDataWidget.cpp
        NewDataWidget/NewDataWidget.ui
        )

add_library(DataManager_Widget STATIC ${DATA_MANAGER_WIDGET_SOURCES})


set_target_properties(DataManager_Widget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(DataManager_Widget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
)

target_link_libraries(DataManager_Widget PRIVATE DataManager DataManagerIO TensorData)
target_link_libraries(DataManager_Widget PRIVATE EditorState)
target_link_libraries(DataManager_Widget PRIVATE MediaExport)
target_link_libraries(DataManager_Widget PRIVATE Feature_Table_Widget)
target_link_libraries(DataManager_Widget PRIVATE IO_Widget)
target_link_libraries(DataManager_Widget PRIVATE Collapsible_Widget)
target_link_libraries(DataManager_Widget PRIVATE Entity)
target_link_libraries(DataManager_Widget PRIVATE GroupManagementWidget)

target_link_libraries(DataManager_Widget PRIVATE nlohmann_json::nlohmann_json)


target_include_directories(DataManager_Widget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(DataManager_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DataManager_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(DataManager_Widget PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(DataManager_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
) 