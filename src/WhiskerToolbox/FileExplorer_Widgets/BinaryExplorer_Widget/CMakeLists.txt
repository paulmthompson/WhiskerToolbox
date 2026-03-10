# BinaryExplorer_Widget Library
# Provides a Qt widget for browsing and previewing raw binary data files.
# Users can configure header offset, data type, channels, byte order,
# and bitwise expansion mode for digital event data.
#
# Key components:
# - BinaryExplorer_Widget: Main widget with config controls, metadata, and preview table
# - BinaryDatasetPreviewModel: Lazy-loading table model for binary data
# - BinaryExplorerRegistration: Registration with DataImportTypeRegistry
#
# Dependencies:
# - Qt6: Widgets, Core, Gui
# - DataImport_Widget: For type registry integration
# - DataManager: For future data import integration

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(BINARY_EXPLORER_WIDGET_SOURCES
    BinaryExplorer_Widget.cpp
    BinaryExplorer_Widget.hpp
    BinaryExplorer_Widget.ui
    BinaryExplorerRegistration.cpp
    BinaryExplorerRegistration.hpp
    BinaryDatasetPreviewModel.cpp
    BinaryDatasetPreviewModel.hpp
)

add_library(BinaryExplorer_Widget STATIC ${BINARY_EXPLORER_WIDGET_SOURCES})

target_link_libraries(BinaryExplorer_Widget
    PUBLIC
        Qt6::Widgets
        Qt6::Core
        Qt6::Gui
    PRIVATE
        DataImport_Widget  # For DataImportTypeRegistry
        DataManager        # For future data import
        WhiskerToolbox::StateManagement
)

target_include_directories(BinaryExplorer_Widget
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(BinaryExplorer_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(BinaryExplorer_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(BinaryExplorer_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(BinaryExplorer_Widget PRIVATE ${MSVC_WARNINGS})
endif()

add_library(WhiskerToolbox::BinaryExplorer_Widget ALIAS BinaryExplorer_Widget)
enable_whiskertoolbox_shared_pch(BinaryExplorer_Widget)