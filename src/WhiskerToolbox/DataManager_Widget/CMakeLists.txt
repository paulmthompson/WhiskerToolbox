find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)
find_package(reflectcpp CONFIG REQUIRED)

set(DATA_MANAGER_WIDGET_SOURCES
        DataManager_Widget.hpp
        DataManager_Widget.cpp
        DataManager_Widget.ui
        DataManagerWidgetState.hpp
        DataManagerWidgetState.cpp
        DataManagerWidgetRegistration.hpp
        DataManagerWidgetRegistration.cpp

        OutputDirectoryWidget/OutputDirectoryWidget.hpp
        OutputDirectoryWidget/OutputDirectoryWidget.ui
        OutputDirectoryWidget/OutputDirectoryWidget.cpp

        NewDataWidget/NewDataWidget.hpp
        NewDataWidget/NewDataWidget.cpp
        NewDataWidget/NewDataWidget.ui

        NewTimeFrameWidget/NewTimeFrameWidget.hpp
        NewTimeFrameWidget/NewTimeFrameWidget.cpp
        NewTimeFrameWidget/NewTimeFrameWidget.ui
        )

add_library(DataManager_Widget STATIC ${DATA_MANAGER_WIDGET_SOURCES})


set_target_properties(DataManager_Widget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(DataManager_Widget PUBLIC 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
)

target_link_libraries(DataManager_Widget PUBLIC reflectcpp::reflectcpp)
target_link_libraries(DataManager_Widget PUBLIC EditorState)

target_link_libraries(DataManager_Widget PRIVATE DataManager)
target_link_libraries(DataManager_Widget PRIVATE TensorData)
target_link_libraries(DataManager_Widget PRIVATE AnalogTimeSeries)
target_link_libraries(DataManager_Widget PRIVATE DigitalTimeSeries)
target_link_libraries(DataManager_Widget PRIVATE PointData)
target_link_libraries(DataManager_Widget PRIVATE LineData)
target_link_libraries(DataManager_Widget PRIVATE MaskData)
target_link_libraries(DataManager_Widget PRIVATE MediaData)

target_link_libraries(DataManager_Widget PRIVATE NEURALYZER_GEOMETRY)

target_link_libraries(DataManager_Widget PRIVATE Feature_Table_Widget)
target_link_libraries(DataManager_Widget PRIVATE TimeFrame_Table_Widget)

target_link_libraries(DataManager_Widget PRIVATE Collapsible_Widget)
target_link_libraries(DataManager_Widget PRIVATE Entity)
target_link_libraries(DataManager_Widget PRIVATE GroupManagementWidget)

target_link_libraries(DataManager_Widget PRIVATE WhiskerToolbox::StateManagement)


target_include_directories(DataManager_Widget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_compiler_warnings(DataManager_Widget)

# Enable automoc for Qt
set_target_properties(DataManager_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
enable_whiskertoolbox_shared_pch(DataManager_Widget)