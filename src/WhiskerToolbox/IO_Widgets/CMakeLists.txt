
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(IO_WIDGET_SOURCES
        AnalogTimeSeries/Analog_Loader_Widget.hpp
        AnalogTimeSeries/Analog_Loader_Widget.cpp
        AnalogTimeSeries/Analog_Loader_Widget.ui
        AnalogTimeSeries/CSV/CSVAnalogLoader_Widget.cpp
        AnalogTimeSeries/CSV/CSVAnalogLoader_Widget.hpp
        AnalogTimeSeries/CSV/CSVAnalogLoader_Widget.ui
        AnalogTimeSeries/CSV/CSVAnalogSaver_Widget.cpp
        AnalogTimeSeries/CSV/CSVAnalogSaver_Widget.hpp
        AnalogTimeSeries/CSV/CSVAnalogSaver_Widget.ui

        Points/Point_Loader_Widget.hpp
        Points/Point_Loader_Widget.cpp
        Points/Point_Loader_Widget.ui
        Points/CSV/CSVPointLoader_Widget.cpp
        Points/CSV/CSVPointLoader_Widget.hpp
        Points/CSV/CSVPointLoader_Widget.ui
        Points/CSV/CSVPointSaver_Widget.cpp
        Points/CSV/CSVPointSaver_Widget.hpp
        Points/CSV/CSVPointSaver_Widget.ui

        Masks/Mask_Loader_Widget.hpp
        Masks/Mask_Loader_Widget.cpp
        Masks/Mask_Loader_Widget.ui
        Masks/HDF5/HDF5MaskLoader_Widget.cpp
        Masks/HDF5/HDF5MaskLoader_Widget.hpp
        Masks/HDF5/HDF5MaskLoader_Widget.ui
        Masks/HDF5/HDF5MaskSaver_Widget.cpp
        Masks/HDF5/HDF5MaskSaver_Widget.hpp
        Masks/HDF5/HDF5MaskSaver_Widget.ui
        Masks/Image/ImageMaskLoader_Widget.cpp
        Masks/Image/ImageMaskLoader_Widget.hpp
        Masks/Image/ImageMaskLoader_Widget.ui
        Masks/Image/ImageMaskSaver_Widget.cpp
        Masks/Image/ImageMaskSaver_Widget.hpp
        Masks/Image/ImageMaskSaver_Widget.ui

        Lines/Line_Loader_Widget.hpp
        Lines/Line_Loader_Widget.cpp
        Lines/Line_Loader_Widget.ui
        Lines/HDF5/HDF5LineLoader_Widget.hpp
        Lines/HDF5/HDF5LineLoader_Widget.cpp
        Lines/HDF5/HDF5LineLoader_Widget.ui
        Lines/CSV/CSVLineLoader_Widget.hpp
        Lines/CSV/CSVLineLoader_Widget.cpp
        Lines/CSV/CSVLineLoader_Widget.ui
        Lines/CSV/CSVLineSaver_Widget.cpp
        Lines/CSV/CSVLineSaver_Widget.hpp
        Lines/CSV/CSVLineSaver_Widget.ui
        Lines/LMDB/LMDBLineLoader_Widget.hpp
        Lines/LMDB/LMDBLineLoader_Widget.cpp
        Lines/LMDB/LMDBLineLoader_Widget.ui

        DigitalTimeSeries/Digital_Interval_Loader_Widget.hpp
        DigitalTimeSeries/Digital_Interval_Loader_Widget.cpp
        DigitalTimeSeries/Digital_Interval_Loader_Widget.ui
        DigitalTimeSeries/CSV/CSVDigitalIntervalLoader_Widget.cpp
        DigitalTimeSeries/CSV/CSVDigitalIntervalLoader_Widget.hpp
        DigitalTimeSeries/CSV/CSVDigitalIntervalLoader_Widget.ui
        DigitalTimeSeries/CSV/CSVIntervalSaver_Widget.cpp
        DigitalTimeSeries/CSV/CSVIntervalSaver_Widget.hpp
        DigitalTimeSeries/CSV/CSVIntervalSaver_Widget.ui

        DigitalTimeSeries/Digital_Event_Loader_Widget.cpp
        DigitalTimeSeries/Digital_Event_Loader_Widget.hpp
        DigitalTimeSeries/Digital_Event_Loader_Widget.ui
        DigitalTimeSeries/CSV/CSVDigitalEventLoader_Widget.cpp
        DigitalTimeSeries/CSV/CSVDigitalEventLoader_Widget.hpp
        DigitalTimeSeries/CSV/CSVDigitalEventLoader_Widget.ui
        DigitalTimeSeries/CSV/CSVEventSaver_Widget.cpp
        DigitalTimeSeries/CSV/CSVEventSaver_Widget.hpp
        DigitalTimeSeries/CSV/CSVEventSaver_Widget.ui

        Tensors/Tensor_Loader_Widget.hpp
        Tensors/Tensor_Loader_Widget.cpp
        Tensors/Tensor_Loader_Widget.ui
)

# Conditionally add CapnProto-dependent Binary widget sources
if(ENABLE_CAPNPROTO)
    list(APPEND IO_WIDGET_SOURCES

        Lines/Binary/BinaryLineSaver_Widget.cpp
        Lines/Binary/BinaryLineSaver_Widget.hpp
        Lines/Binary/BinaryLineSaver_Widget.ui
        Lines/Binary/BinaryLineLoader_Widget.cpp
        Lines/Binary/BinaryLineLoader_Widget.hpp
        Lines/Binary/BinaryLineLoader_Widget.ui

        AnalogTimeSeries/Binary/BinaryAnalogLoader_Widget.cpp
        AnalogTimeSeries/Binary/BinaryAnalogLoader_Widget.hpp
        AnalogTimeSeries/Binary/BinaryAnalogLoader_Widget.ui
        
        DigitalTimeSeries/Binary/BinaryDigitalIntervalLoader_Widget.cpp
        DigitalTimeSeries/Binary/BinaryDigitalIntervalLoader_Widget.hpp
        DigitalTimeSeries/Binary/BinaryDigitalIntervalLoader_Widget.ui
    )
endif()


add_library(IO_Widget STATIC ${IO_WIDGET_SOURCES})

set_target_properties(IO_Widget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(IO_Widget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
)

target_link_libraries(IO_Widget PRIVATE DataManager)
target_link_libraries(IO_Widget PRIVATE Scaling_Widget)  # Shared library for image scaling

target_include_directories(IO_Widget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(IO_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(IO_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(IO_Widget PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(IO_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
) 