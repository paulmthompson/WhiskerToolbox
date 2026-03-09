# NpyExplorer_Widget Library
# Provides a Qt widget for browsing and previewing NumPy .npy files.
# Shows file metadata (dtype, shape, byte order) and a lazy-loading data preview.
#
# Key components:
# - NpyExplorer_Widget: Main widget with metadata and preview table
# - NpyDatasetPreviewModel: Lazy-loading table model for .npy data
# - NpyExplorerRegistration: Registration with DataImportTypeRegistry
#
# Dependencies:
# - Qt6: Widgets, Core, Gui
# - libnpy: Header-only library for reading .npy format
# - DataImport_Widget: For type registry integration
# - DataManager: For future data import integration

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(NPY_EXPLORER_WIDGET_SOURCES
    NpyExplorer_Widget.cpp
    NpyExplorer_Widget.hpp
    NpyExplorer_Widget.ui
    NpyExplorerRegistration.cpp
    NpyExplorerRegistration.hpp
    NpyDatasetPreviewModel.cpp
    NpyDatasetPreviewModel.hpp
)

add_library(NpyExplorer_Widget STATIC ${NPY_EXPLORER_WIDGET_SOURCES})

target_link_libraries(NpyExplorer_Widget
    PUBLIC
        Qt6::Widgets
        Qt6::Core
        Qt6::Gui
    PRIVATE
        DataImport_Widget  # For DataImportTypeRegistry
        DataManager        # For future data import
        WhiskerToolbox::StateManagement
)

target_include_directories(NpyExplorer_Widget
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
    PRIVATE
        "${npy_SOURCE_DIR}/include"  # For libnpy header-only library
)

set_target_properties(NpyExplorer_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(NpyExplorer_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(NpyExplorer_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(NpyExplorer_Widget PRIVATE ${MSVC_WARNINGS})
endif()

add_library(WhiskerToolbox::NpyExplorer_Widget ALIAS NpyExplorer_Widget)
enable_whiskertoolbox_pch(NpyExplorer_Widget)