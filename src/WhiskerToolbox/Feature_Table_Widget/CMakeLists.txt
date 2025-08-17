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

# Link Qt libraries
target_link_libraries(Feature_Table_Widget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
)

# Link against DataManager and Color_Widget
target_link_libraries(Feature_Table_Widget PRIVATE DataManager)

# Include directories
target_include_directories(Feature_Table_Widget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compile definitions
target_compile_definitions(Feature_Table_Widget PRIVATE _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(Feature_Table_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(Feature_Table_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(Feature_Table_Widget PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(Feature_Table_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
) 