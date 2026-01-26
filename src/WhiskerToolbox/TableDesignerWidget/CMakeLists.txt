
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Concurrent)
find_package(reflectcpp CONFIG REQUIRED)

# Create the library
add_library(TableDesignerWidget STATIC
    TableDesignerWidget.hpp
    TableDesignerWidget.cpp
    TableDesignerWidget.ui
    TableDesignerState.hpp
    TableDesignerState.cpp
    TableDesignerStateData.hpp
    TableDesignerWidgetRegistration.hpp
    TableDesignerWidgetRegistration.cpp
    PreviewTableModel.hpp
    PreviewTableModel.cpp
    TableInfoWidget.hpp
    TableInfoWidget.cpp
    TableInfoWidget.ui
    TableTransformWidget.hpp
    TableTransformWidget.cpp
    TableTransformWidget.ui
    TableExportWidget.hpp
    TableExportWidget.cpp
    TableExportWidget.ui
    TableJSONWidget.hpp
    TableJSONWidget.cpp
    TableJSONWidget.ui
)

# Link Qt6 and other dependencies
target_link_libraries(TableDesignerWidget
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        Qt6::Concurrent
        DataManager
        TableViewerWidget
        Collapsible_Widget
        EditorState
        reflectcpp::reflectcpp
)

# Include directories
target_include_directories(TableDesignerWidget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Enable Qt MOC for this target
set_target_properties(TableDesignerWidget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
)
