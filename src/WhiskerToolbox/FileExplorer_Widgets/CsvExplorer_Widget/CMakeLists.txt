# CsvExplorer_Widget Library
# Provides a Qt widget for browsing and previewing CSV/delimited text files.
# Users can configure column delimiter, header lines, quote character,
# and whitespace trimming for flexible parsing of various text formats.
#
# Key components:
# - CsvExplorer_Widget: Main widget with config controls, metadata, and preview table
# - CsvDatasetPreviewModel: Lazy-loading table model for CSV data
# - CsvExplorerRegistration: Registration with DataImportTypeRegistry
#
# Dependencies:
# - Qt6: Widgets, Core, Gui
# - DataImport_Widget: For type registry integration
# - DataManager: For future data import integration

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(CSV_EXPLORER_WIDGET_SOURCES
    CsvExplorer_Widget.cpp
    CsvExplorer_Widget.hpp
    CsvExplorer_Widget.ui
    CsvExplorerRegistration.cpp
    CsvExplorerRegistration.hpp
    CsvDatasetPreviewModel.cpp
    CsvDatasetPreviewModel.hpp
)

add_library(CsvExplorer_Widget STATIC ${CSV_EXPLORER_WIDGET_SOURCES})

target_link_libraries(CsvExplorer_Widget
    PUBLIC
        Qt6::Widgets
        Qt6::Core
        Qt6::Gui
    PRIVATE
        DataImport_Widget  # For DataImportTypeRegistry
        DataManager        # For future data import
        WhiskerToolbox::StateManagement
)

target_include_directories(CsvExplorer_Widget
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(CsvExplorer_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

set_target_compiler_warnings(CsvExplorer_Widget)

add_library(WhiskerToolbox::CsvExplorer_Widget ALIAS CsvExplorer_Widget)
enable_whiskertoolbox_shared_pch(CsvExplorer_Widget)