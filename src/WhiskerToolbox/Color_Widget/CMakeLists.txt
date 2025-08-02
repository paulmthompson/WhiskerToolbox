# Color_Widget Library CMakeLists.txt

# Find required packages
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

# Define the Color_Widget library sources
set(COLOR_WIDGET_SOURCES
    Color_Widget.hpp
)

# Create the Color_Widget library (header-only for now)
add_library(Color_Widget INTERFACE)

# Set target properties
set_target_properties(Color_Widget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Link Qt libraries
target_link_libraries(Color_Widget INTERFACE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
)

# Include directories
target_include_directories(Color_Widget INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compile definitions
target_compile_definitions(Color_Widget INTERFACE _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH) 