
find_package(HDF5 COMPONENTS CXX REQUIRED)

set(CMAKE_CUDA_FLAGS "-allow-unsupported-compiler")

find_package(Torch REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

find_package(nlohmann_json CONFIG REQUIRED)

#find_package(unofficial-lmdb CONFIG REQUIRED)
find_package(CapnProto CONFIG REQUIRED)
#find_package(RocksDB CONFIG REQUIRED)

find_package(LAPACK REQUIRED)

# Add Observer subdirectory to build the ObserverData library
add_subdirectory(Observer)

set(DATAMANAGER_SOURCES
        DataManager.hpp
        DataManager.cpp
        DataManagerTypes.hpp
        TimeFrame.hpp
        TimeFrame.cpp
        ImageSize/ImageSize.hpp

        Media/Image_Data.cpp
        Media/Image_Data.hpp
        Media/Video_Data.cpp
        Media/Video_Data.hpp
        Media/Media_Data.hpp
        Media/Media_Data.cpp
        Media/HDF5_Data.hpp
        Media/HDF5_Data.cpp
        Media/Video_Data_Loader.hpp

        DigitalTimeSeries/Digital_Interval_Series.hpp
        DigitalTimeSeries/Digital_Interval_Series.cpp
        DigitalTimeSeries/IO/CSV/Digital_Interval_Series_CSV.hpp
        DigitalTimeSeries/IO/CSV/Digital_Interval_Series_CSV.cpp
        DigitalTimeSeries/IO/Binary/Digital_Interval_Series_Binary.hpp
        DigitalTimeSeries/IO/Binary/Digital_Interval_Series_Binary.cpp
        DigitalTimeSeries/IO/JSON/Digital_Interval_Series_JSON.hpp
        DigitalTimeSeries/IO/JSON/Digital_Interval_Series_JSON.cpp
        DigitalTimeSeries/interval_data.hpp
        DigitalTimeSeries/Digital_Event_Series.hpp
        DigitalTimeSeries/Digital_Event_Series.cpp
        DigitalTimeSeries/IO/CSV/Digital_Event_Series_CSV.cpp
        DigitalTimeSeries/IO/CSV/Digital_Event_Series_CSV.hpp
        DigitalTimeSeries/IO/JSON/Digital_Event_Series_JSON.hpp
        DigitalTimeSeries/IO/JSON/Digital_Event_Series_JSON.cpp

        Tensors/Tensor_Data.hpp
        Tensors/Tensor_Data.cpp

        loaders/CSV_Loaders.cpp
        loaders/CSV_Loaders.hpp
        loaders/binary_loaders.hpp
        loaders/hdf5_loaders.hpp
        loaders/hdf5_loaders.cpp
        loaders/loading_utils.hpp
        loaders/loading_utils.cpp

        utils/armadillo_wrap/analog_armadillo.hpp
        utils/armadillo_wrap/analog_armadillo.cpp
        utils/color.hpp
        utils/color.cpp
        utils/hdf5_mask_load.hpp
        utils/json_helpers.hpp
        utils/opencv_utility.hpp
        utils/opencv_utility.cpp
        utils/polynomial/polynomial_fit.hpp
        utils/polynomial/polynomial_fit.cpp
        utils/polynomial/parametric_polynomial_utils.cpp
        utils/polynomial/parametric_polynomial_utils.hpp
        utils/ProcessingOptions.hpp
        utils/string_manip.hpp

        transforms/data_transforms.cpp
        transforms/data_transforms.hpp
        transforms/TransformRegistry.hpp
        transforms/TransformRegistry.cpp
        transforms/AnalogTimeSeries/analog_event_threshold.cpp
        transforms/AnalogTimeSeries/analog_event_threshold.hpp
        transforms/AnalogTimeSeries/analog_interval_threshold.cpp
        transforms/AnalogTimeSeries/analog_interval_threshold.hpp
        transforms/AnalogTimeSeries/analog_hilbert_phase.hpp
        transforms/AnalogTimeSeries/analog_hilbert_phase.cpp
        transforms/AnalogTimeSeries/analog_scaling.cpp
        transforms/AnalogTimeSeries/analog_scaling.hpp
        transforms/Lines/line_angle.hpp
        transforms/Lines/line_angle.cpp
        transforms/Lines/line_min_point_dist.hpp
        transforms/Lines/line_min_point_dist.cpp
        transforms/Lines/line_resample.cpp
        transforms/Lines/line_resample.hpp
        transforms/Lines/line_curvature.cpp
        transforms/Lines/line_curvature.hpp
        transforms/Lines/line_subsegment.cpp
        transforms/Lines/line_subsegment.hpp
        transforms/Lines/line_point_extraction.cpp
        transforms/Lines/line_point_extraction.hpp
        transforms/Lines/line_clip.cpp
        transforms/Lines/line_clip.hpp
        transforms/Masks/mask_area.cpp
        transforms/Masks/mask_area.hpp
        transforms/Masks/mask_to_line.cpp
        transforms/Masks/mask_to_line.hpp
        transforms/Masks/mask_skeletonize.cpp
        transforms/Masks/mask_skeletonize.hpp
)


set_property(GLOBAL PROPERTY DATAMANAGER_TEST_SOURCES "")

add_subdirectory(AnalogTimeSeries)
add_subdirectory(Lines)
add_subdirectory(Masks)
add_subdirectory(Points)

add_library(DataManager SHARED ${DATAMANAGER_SOURCES}
    )

message(STATUS "NPY source dir:")
message(STATUS ${npy_SOURCE_DIR})

target_include_directories(DataManager PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
        "${npy_SOURCE_DIR}/include"
    )


target_link_libraries(DataManager PRIVATE ffmpeg_wrapper::ffmpeg_wrapper)

target_link_libraries(DataManager PRIVATE HighFive)

if (APPLE)
    target_link_libraries(DataManager PRIVATE hdf5::hdf5-static hdf5::hdf5_cpp-static)
else()
    target_link_libraries(DataManager PRIVATE hdf5::hdf5-shared hdf5::hdf5_cpp-shared)
endif()

target_link_libraries(DataManager PRIVATE opencv_core opencv_imgproc opencv_imgcodecs opencv_photo)

target_link_libraries(DataManager PRIVATE armadillo)
target_link_libraries(DataManager PRIVATE LAPACK::LAPACK)

target_link_libraries(DataManager PUBLIC nlohmann_json::nlohmann_json)
#target_link_libraries(DataManager PRIVATE unofficial::lmdb::lmdb)
target_link_libraries(DataManager PRIVATE CapnProto::capnp)
#target_link_libraries(DataManager PRIVATE RocksDB::rocksdb RocksDB::rocksdb-shared)

target_link_libraries(DataManager PRIVATE "${TORCH_LIBRARIES}")

# Link to the ObserverData library
target_link_libraries(DataManager PUBLIC WhiskerToolbox::ObserverData)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(DataManager PRIVATE ${CLANG_OPTIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DataManager PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(DataManager PRIVATE ${MSVC_WARNINGS})
endif()

set_target_properties(DataManager PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
