# Feature_Table_Widget Library CMakeLists.txt

# Find required packages
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

# Define the Feature_Table_Widget library sources
set(FEATURE_TABLE_WIDGET_SOURCES
    Feature_Table_Widget.hpp
    Feature_Table_Widget.cpp
    Feature_Table_Widget.ui
)

# Create the Feature_Table_Widget library
add_library(Feature_Table_Widget STATIC ${FEATURE_TABLE_WIDGET_SOURCES})

# Set target properties
set_target_properties(Feature_Table_Widget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(Feature_Table_Widget PUBLIC 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
)

target_link_libraries(Feature_Table_Widget PUBLIC DataManager)

# Include directories
target_include_directories(Feature_Table_Widget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_compiler_warnings(Feature_Table_Widget)

# Enable automoc for Qt
set_target_properties(Feature_Table_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
) 
enable_whiskertoolbox_shared_pch(Feature_Table_Widget)