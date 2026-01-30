find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(DATA_INSPECTOR_WIDGET_SOURCES
        DataInspectorState.hpp
        DataInspectorState.cpp
        DataInspectorWidgetRegistration.hpp
        DataInspectorWidgetRegistration.cpp
        DataInspectorPropertiesWidget.hpp
        DataInspectorPropertiesWidget.cpp
        DataInspectorPropertiesWidget.ui
        DataInspectorViewWidget.hpp
        DataInspectorViewWidget.cpp
        DataInspectorViewWidget.ui
        
        # Inspector interface, base, and factory
        Inspectors/IDataInspector.hpp
        Inspectors/BaseInspector.hpp
        Inspectors/BaseInspector.cpp
        Inspectors/InspectorFactory.hpp
        Inspectors/InspectorFactory.cpp
        
        # View interface, base, and factory (Phase 3)
        Inspectors/IDataView.hpp
        Inspectors/BaseDataView.hpp
        Inspectors/BaseDataView.cpp
        Inspectors/ViewFactory.hpp
        Inspectors/ViewFactory.cpp
        
        # Type-specific inspectors and views
        AnalogTimeSeries/AnalogTimeSeriesInspector.hpp
        AnalogTimeSeries/AnalogTimeSeriesInspector.cpp
        AnalogTimeSeries/AnalogTimeSeriesInspector.ui
        AnalogTimeSeries/AnalogTimeSeriesDataView.hpp
        AnalogTimeSeries/AnalogTimeSeriesDataView.cpp

        DigitalEventSeries/DigitalEventSeriesInspector.hpp
        DigitalEventSeries/DigitalEventSeriesInspector.cpp
        DigitalEventSeries/DigitalEventSeriesInspector.ui
        DigitalEventSeries/EventTableModel.hpp
        DigitalEventSeries/DigitalEventSeriesDataView.hpp
        DigitalEventSeries/DigitalEventSeriesDataView.cpp

        DigitalIntervalSeries/DigitalIntervalSeriesInspector.hpp
        DigitalIntervalSeries/DigitalIntervalSeriesInspector.cpp
        DigitalIntervalSeries/DigitalIntervalSeriesInspector.ui
        DigitalIntervalSeries/IntervalTableModel.hpp
        DigitalIntervalSeries/DigitalIntervalSeriesDataView.hpp
        DigitalIntervalSeries/DigitalIntervalSeriesDataView.cpp

        ImageData/ImageInspector.hpp
        ImageData/ImageInspector.cpp
        ImageData/ImageTableModel.cpp
        ImageData/ImageTableModel.hpp
        ImageData/ImageDataView.hpp
        ImageData/ImageDataView.cpp

        LineData/LineInspector.hpp
        LineData/LineInspector.cpp
        LineData/LineInspector.ui
        LineData/LineTableModel.cpp
        LineData/LineTableModel.hpp
        LineData/LineTableView.hpp
        LineData/LineTableView.cpp

        MaskData/MaskInspector.hpp
        MaskData/MaskInspector.cpp
        MaskData/Mask_Widget.hpp
        MaskData/Mask_Widget.cpp
        MaskData/Mask_Widget.ui
        MaskData/MaskTableModel.cpp
        MaskData/MaskTableModel.hpp
        MaskData/MaskTableView.hpp
        MaskData/MaskTableView.cpp

        PointData/PointInspector.hpp
        PointData/PointInspector.cpp
        PointData/Point_Widget.hpp
        PointData/Point_Widget.cpp
        PointData/Point_Widget.ui
        PointData/PointTableModel.cpp
        PointData/PointTableModel.hpp
        PointData/PointTableView.hpp
        PointData/PointTableView.cpp

        TensorData/TensorInspector.hpp
        TensorData/TensorInspector.cpp
        TensorData/TensorTableModel.hpp
        TensorData/TensorDataView.hpp
        TensorData/TensorDataView.cpp
        )

add_library(DataInspector_Widget STATIC ${DATA_INSPECTOR_WIDGET_SOURCES})

set_target_properties(DataInspector_Widget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(DataInspector_Widget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
)

# Core dependencies
target_link_libraries(DataInspector_Widget PRIVATE DataManager)
target_link_libraries(DataInspector_Widget PRIVATE EditorState)
target_link_libraries(DataInspector_Widget PRIVATE GroupManagementWidget)

# Dependencies for type-specific inspectors (they wrap existing widgets)
target_link_libraries(DataInspector_Widget PRIVATE DataManager_Widget)

# JSON libraries for serialization
target_link_libraries(DataInspector_Widget PRIVATE nlohmann_json::nlohmann_json)

target_include_directories(DataInspector_Widget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" # This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>" # Parent for sibling widgets
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" # For DataManager etc
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(DataInspector_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DataInspector_Widget PRIVATE ${GCC_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(DataInspector_Widget PRIVATE ${MSVC_OPTIONS})
endif()
