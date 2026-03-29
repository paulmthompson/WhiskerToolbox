# IO Plugin System CMakeLists.txt

find_package(nlohmann_json CONFIG REQUIRED)

# ── Interface library for headers shared between DataManagerIO and format plugins ──
# This provides IFormatLoader, DataLoader, SaverInfo, IOFormats without any
# export macros, breaking the circular dependency between DataManagerIO and its
# plugin shared libraries.
add_library(DataManagerIO_Core INTERFACE)

target_include_directories(DataManagerIO_Core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

target_link_libraries(DataManagerIO_Core INTERFACE
    nlohmann_json::nlohmann_json
    DataTypeEnum
    ParameterSchema
)

# Core plugin interface
set(IO_CORE_SOURCES
    core/DataLoader.hpp
    core/IOFormats.hpp
    core/SaverInfo.hpp
    core/AtomicWrite.hpp
    core/AtomicWrite.cpp

    core/LoaderRegistry.hpp
    core/LoaderRegistry.cpp
    core/LoaderRegistration.hpp
    core/LoaderRegistration.cpp

    # Format-centric loaders (unified architecture)
    formats/CSV/CSVLoader.hpp
    formats/CSV/CSVLoader.cpp
    formats/CSV/analogtimeseries/Analog_Time_Series_CSV.hpp
    formats/CSV/analogtimeseries/Analog_Time_Series_CSV.cpp
    formats/CSV/common/CSV_Loaders.hpp
    formats/CSV/common/CSV_Loaders.cpp
    formats/CSV/digitaltimeseries/Digital_Event_Series_CSV.hpp
    formats/CSV/digitaltimeseries/Digital_Event_Series_CSV.cpp
    formats/CSV/digitaltimeseries/Digital_Interval_Series_CSV.hpp
    formats/CSV/digitaltimeseries/Digital_Interval_Series_CSV.cpp
    formats/CSV/digitaltimeseries/MultiColumnBinaryCSV.hpp
    formats/CSV/digitaltimeseries/MultiColumnBinaryCSV.cpp
    formats/Binary/BinaryFormatLoader.hpp
    formats/Binary/BinaryFormatLoader.cpp
    formats/Binary/analogtimeseries/Analog_Time_Series_Binary.hpp
    formats/Binary/analogtimeseries/Analog_Time_Series_Binary.cpp
    formats/Binary/digitaltimeseries/Digital_Interval_Series_Binary.hpp
    formats/Binary/digitaltimeseries/Digital_Interval_Series_Binary.cpp
    formats/CSV/mask/Mask_Data_CSV.hpp
    formats/CSV/mask/Mask_Data_CSV.cpp
    formats/CSV/points/Point_Data_CSV.hpp
    formats/CSV/points/Point_Data_CSV.cpp
    formats/CSV/lines/Line_Data_CSV.hpp
    formats/CSV/lines/Line_Data_CSV.cpp
    formats/CSV/tensors/Tensor_Data_CSV.hpp
    formats/CSV/tensors/Tensor_Data_CSV.cpp
)


add_library(DataManagerIO SHARED ${IO_CORE_SOURCES})

include(GenerateExportHeader)

set_target_properties(DataManagerIO
    PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)

generate_export_header(DataManagerIO
                       EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/datamanagerio_export.h)

target_include_directories(DataManagerIO PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR} # For generated export header
)

target_sources(
  DataManagerIO PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
    FILES ${CMAKE_CURRENT_BINARY_DIR}/datamanagerio_export.h
)


target_link_libraries(DataManagerIO PUBLIC
    DataManagerIO_Core
    nlohmann_json::nlohmann_json
    NEURALYZER_GEOMETRY
    ParameterSchema
    DataTypeEnum
    LineData # Needed to populate CSV line data
    PointData # Needed for Point loaders
    MaskData # Needed for Mask RLE CSV loaders
    AnalogTimeSeries # Needed for Analog loaders
    DigitalTimeSeries # Needed for Digital loaders
    TensorData # Needed for Tensor CSV loaders
)

target_link_libraries(DataManagerIO PRIVATE CoreUtilities)

# Add subdirectories for specific format plugins
if(ENABLE_HDF5)
    add_subdirectory(formats/HDF5)
    target_compile_definitions(DataManagerIO PUBLIC ENABLE_HDF5)
    target_link_libraries(DataManagerIO PRIVATE DataManagerHDF5)
endif()

if(ENABLE_CAPNPROTO)
    add_subdirectory(formats/CapnProto)
    target_compile_definitions(DataManagerIO PUBLIC ENABLE_CAPNPROTO)
    target_link_libraries(DataManagerIO PRIVATE DataManagerIO_CapnProto)
endif()

if(ENABLE_OPENCV)
    add_subdirectory(formats/OpenCV)
    target_compile_definitions(DataManagerIO PUBLIC ENABLE_OPENCV)
    target_link_libraries(DataManagerIO PRIVATE DataManagerOpenCV)
endif()

# Numpy is always available (header-only libnpy fetched via FetchContent)
add_subdirectory(formats/Numpy)
target_link_libraries(DataManagerIO PRIVATE DataManagerNumpy)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(DataManagerIO PRIVATE ${CLANG_OPTIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DataManagerIO PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(DataManagerIO PRIVATE ${MSVC_WARNINGS})
endif()

# Prevent static-archive symbols (e.g. TimeFrame) from leaking into the .so
target_link_options(DataManagerIO PRIVATE
    $<$<CXX_COMPILER_ID:Clang,GNU>:LINKER:--exclude-libs,ALL>)

# Conditionally define preprocessor macros based on actual library availability
if(ENABLE_CAPNPROTO)

endif()

if(ENABLE_HDF5)

endif()

if(ENABLE_OPENCV)

endif()


# Set properties
set_target_properties(DataManagerIO PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

enable_whiskertoolbox_pch(DataManagerIO)
