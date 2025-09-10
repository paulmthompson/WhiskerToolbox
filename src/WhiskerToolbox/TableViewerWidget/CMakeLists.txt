# TableViewerWidget Library
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# Create the library
add_library(TableViewerWidget STATIC
    TableViewerWidget.hpp
    TableViewerWidget.cpp
    TableViewerWidget.ui
    PaginatedTableModel.hpp
    PaginatedTableModel.cpp
)

# Link Qt6 and other dependencies
target_link_libraries(TableViewerWidget
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        DataManager
)

# Include directories
target_include_directories(TableViewerWidget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Enable Qt MOC for this target
set_target_properties(TableViewerWidget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
)
