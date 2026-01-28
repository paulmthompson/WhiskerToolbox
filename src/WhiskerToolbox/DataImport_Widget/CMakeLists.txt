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
    DataImportWidgetRegistration.cpp
    DataImportWidgetRegistration.hpp
    
    # Points import widgets
    Points/PointImport_Widget.cpp
    Points/PointImport_Widget.hpp
    Points/PointImport_Widget.ui
    Points/CSV/CSVPointImport_Widget.cpp
    Points/CSV/CSVPointImport_Widget.hpp
    Points/CSV/CSVPointImport_Widget.ui
    
    # Tensors import widgets
    Tensors/TensorImport_Widget.cpp
    Tensors/TensorImport_Widget.hpp
    Tensors/TensorImport_Widget.ui
    
    # DigitalTimeSeries import widgets
    DigitalTimeSeries/DigitalEventImport_Widget.cpp
    DigitalTimeSeries/DigitalEventImport_Widget.hpp
    DigitalTimeSeries/DigitalEventImport_Widget.ui
    DigitalTimeSeries/CSV/CSVDigitalEventImport_Widget.cpp
    DigitalTimeSeries/CSV/CSVDigitalEventImport_Widget.hpp
    DigitalTimeSeries/CSV/CSVDigitalEventImport_Widget.ui
    DigitalTimeSeries/DigitalIntervalImport_Widget.cpp
    DigitalTimeSeries/DigitalIntervalImport_Widget.hpp
    DigitalTimeSeries/DigitalIntervalImport_Widget.ui
    DigitalTimeSeries/CSV/CSVDigitalIntervalImport_Widget.cpp
    DigitalTimeSeries/CSV/CSVDigitalIntervalImport_Widget.hpp
    DigitalTimeSeries/CSV/CSVDigitalIntervalImport_Widget.ui
    DigitalTimeSeries/Binary/BinaryDigitalIntervalImport_Widget.cpp
    DigitalTimeSeries/Binary/BinaryDigitalIntervalImport_Widget.hpp
    DigitalTimeSeries/Binary/BinaryDigitalIntervalImport_Widget.ui
    
    # AnalogTimeSeries import widgets
    AnalogTimeSeries/AnalogImport_Widget.cpp
    AnalogTimeSeries/AnalogImport_Widget.hpp
    AnalogTimeSeries/AnalogImport_Widget.ui
    AnalogTimeSeries/CSV/CSVAnalogImport_Widget.cpp
    AnalogTimeSeries/CSV/CSVAnalogImport_Widget.hpp
    AnalogTimeSeries/CSV/CSVAnalogImport_Widget.ui
    AnalogTimeSeries/Binary/BinaryAnalogImport_Widget.cpp
    AnalogTimeSeries/Binary/BinaryAnalogImport_Widget.hpp
    AnalogTimeSeries/Binary/BinaryAnalogImport_Widget.ui
    
    # Masks import widgets
    Masks/MaskImport_Widget.cpp
    Masks/MaskImport_Widget.hpp
    Masks/MaskImport_Widget.ui
    Masks/HDF5/HDF5MaskImport_Widget.cpp
    Masks/HDF5/HDF5MaskImport_Widget.hpp
    Masks/HDF5/HDF5MaskImport_Widget.ui
    Masks/Image/ImageMaskImport_Widget.cpp
    Masks/Image/ImageMaskImport_Widget.hpp
    Masks/Image/ImageMaskImport_Widget.ui
    
    # Lines import widgets
    Lines/LineImport_Widget.cpp
    Lines/LineImport_Widget.hpp
    Lines/LineImport_Widget.ui
    Lines/HDF5/HDF5LineImport_Widget.cpp
    Lines/HDF5/HDF5LineImport_Widget.hpp
    Lines/HDF5/HDF5LineImport_Widget.ui
    Lines/CSV/CSVLineImport_Widget.cpp
    Lines/CSV/CSVLineImport_Widget.hpp
    Lines/CSV/CSVLineImport_Widget.ui
    Lines/Binary/BinaryLineImport_Widget.cpp
    Lines/Binary/BinaryLineImport_Widget.hpp
    Lines/Binary/BinaryLineImport_Widget.ui
    Lines/LMDB/LMDBLineImport_Widget.cpp
    Lines/LMDB/LMDBLineImport_Widget.hpp
    Lines/LMDB/LMDBLineImport_Widget.ui
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
