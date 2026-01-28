
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(DATAEXPORT_WIDGET_SOURCES
        AnalogTimeSeries/CSV/CSVAnalogSaver_Widget.cpp
        AnalogTimeSeries/CSV/CSVAnalogSaver_Widget.hpp
        AnalogTimeSeries/CSV/CSVAnalogSaver_Widget.ui

        Points/CSV/CSVPointSaver_Widget.cpp
        Points/CSV/CSVPointSaver_Widget.hpp
        Points/CSV/CSVPointSaver_Widget.ui

        Masks/HDF5/HDF5MaskSaver_Widget.cpp
        Masks/HDF5/HDF5MaskSaver_Widget.hpp
        Masks/HDF5/HDF5MaskSaver_Widget.ui
        Masks/Image/ImageMaskSaver_Widget.cpp
        Masks/Image/ImageMaskSaver_Widget.hpp
        Masks/Image/ImageMaskSaver_Widget.ui

        Lines/CSV/CSVLineSaver_Widget.cpp
        Lines/CSV/CSVLineSaver_Widget.hpp
        Lines/CSV/CSVLineSaver_Widget.ui

        DigitalTimeSeries/CSV/CSVIntervalSaver_Widget.cpp
        DigitalTimeSeries/CSV/CSVIntervalSaver_Widget.hpp
        DigitalTimeSeries/CSV/CSVIntervalSaver_Widget.ui
        DigitalTimeSeries/CSV/CSVEventSaver_Widget.cpp
        DigitalTimeSeries/CSV/CSVEventSaver_Widget.hpp
        DigitalTimeSeries/CSV/CSVEventSaver_Widget.ui
)

# Conditionally add CapnProto-dependent Binary widget sources
if(ENABLE_CAPNPROTO)
    list(APPEND DATAEXPORT_WIDGET_SOURCES

        Lines/Binary/BinaryLineSaver_Widget.cpp
        Lines/Binary/BinaryLineSaver_Widget.hpp
        Lines/Binary/BinaryLineSaver_Widget.ui
        
    )
endif()


add_library(DataExport_Widget STATIC ${DATAEXPORT_WIDGET_SOURCES})

set_target_properties(DataExport_Widget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(DataExport_Widget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
)

target_link_libraries(DataExport_Widget PRIVATE DataManager)
target_link_libraries(DataExport_Widget PRIVATE Scaling_Widget)  # Shared library for image scaling

target_include_directories(DataExport_Widget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(DataExport_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DataExport_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(DataExport_Widget PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(DataExport_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
) 