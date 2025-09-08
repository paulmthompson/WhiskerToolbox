
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Concurrent)

# Create the library
add_library(TableDesignerWidget STATIC
    TableDesignerWidget.hpp
    TableDesignerWidget.cpp
    TableDesignerWidget.ui
    PreviewTableModel.hpp
    PreviewTableModel.cpp
    TableInfoWidget.hpp
    TableInfoWidget.cpp
    TableInfoWidget.ui
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
