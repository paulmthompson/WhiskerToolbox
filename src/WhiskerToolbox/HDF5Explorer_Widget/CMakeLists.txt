# HDF5Explorer_Widget Library
# Provides a Qt widget for browsing and exploring HDF5 file structure.
# This widget shows the hierarchy of groups and datasets in HDF5 files.
#
# This library is only built when ENABLE_HDF5 is ON.
#
# Key components:
# - HDF5Explorer_Widget: Main widget with tree view of HDF5 structure
# - HDF5ExplorerRegistration: Registration with DataImportTypeRegistry
#
# Dependencies:
# - Qt6: Widgets, Core, Gui
# - HDF5: C++ bindings for HDF5 file access
# - DataImport_Widget: For type registry integration
# - DataManager: For future data import integration

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)
find_package(HDF5 COMPONENTS CXX REQUIRED)

set(HDF5_EXPLORER_WIDGET_SOURCES
    HDF5Explorer_Widget.cpp
    HDF5Explorer_Widget.hpp
    HDF5Explorer_Widget.ui
    HDF5ExplorerRegistration.cpp
    HDF5ExplorerRegistration.hpp
)

add_library(HDF5Explorer_Widget STATIC ${HDF5_EXPLORER_WIDGET_SOURCES})

target_link_libraries(HDF5Explorer_Widget
    PUBLIC
        Qt6::Widgets
        Qt6::Core
        Qt6::Gui
    PRIVATE
        DataImport_Widget  # For DataImportTypeRegistry
        DataManager        # For future data import
)

# Platform-specific HDF5 linking (following the same pattern as DataManagerHDF5)
if (APPLE)
    target_link_libraries(HDF5Explorer_Widget PRIVATE hdf5::hdf5-static hdf5::hdf5_cpp-static)
else()
    target_link_libraries(HDF5Explorer_Widget PRIVATE hdf5::hdf5-shared hdf5::hdf5_cpp-shared)
endif()

target_include_directories(HDF5Explorer_Widget
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

set_target_properties(HDF5Explorer_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(HDF5Explorer_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(HDF5Explorer_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(HDF5Explorer_Widget PRIVATE ${MSVC_WARNINGS})
endif()

add_library(WhiskerToolbox::HDF5Explorer_Widget ALIAS HDF5Explorer_Widget)
