# DataImport_Widget Library
# Provides the UI infrastructure for loading data into DataManager.
# This library decouples import functionality from IO_Widgets (which will
# focus on export/saving functionality).
#
# Key components:
# - DataImportWidgetState: EditorState subclass for serialization
# - DataImportTypeRegistry: Maps data types to loader widget factories
# - DataImport_Widget: Main container with stacked loader widgets
#
# Dependencies:
# - Qt6: Widgets, Core, Gui
# - EditorState: For state management and SelectionContext integration
# - DataManager: For data type access
# - Scaling_Widget: For coordinate transformation on import
# - reflectcpp: For JSON serialization

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)
find_package(reflectcpp CONFIG REQUIRED)

set(DATA_IMPORT_WIDGET_SOURCES
    DataImportWidgetState.cpp
    DataImportWidgetState.hpp
    DataImport_Widget.cpp
    DataImport_Widget.hpp
    DataImport_Widget.ui
    DataImportTypeRegistry.cpp
    DataImportTypeRegistry.hpp
)

add_library(DataImport_Widget STATIC ${DATA_IMPORT_WIDGET_SOURCES})

target_link_libraries(DataImport_Widget
    PUBLIC
        Qt6::Widgets
        Qt6::Core
        Qt6::Gui
        EditorState
        DataManager
        Scaling_Widget
        reflectcpp::reflectcpp
)

target_include_directories(DataImport_Widget
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

set_target_properties(DataImport_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(DataImport_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DataImport_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(DataImport_Widget PRIVATE ${MSVC_WARNINGS})
endif()

add_library(WhiskerToolbox::DataImport_Widget ALIAS DataImport_Widget)
