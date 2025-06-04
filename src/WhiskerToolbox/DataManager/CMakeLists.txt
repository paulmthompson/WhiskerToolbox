
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

add_library(DataManager SHARED
        DataManager.hpp
        DataManager.cpp
        DataManagerTypes.hpp

        TimeFrame.hpp
        TimeFrame.cpp

        Lines/Line_Data.hpp
        Lines/Line_Data.cpp
        Lines/lines.hpp
        Lines/lines.cpp
        Lines/utils/line_geometry.hpp
        Lines/IO/CSV/Line_Data_CSV.hpp
        Lines/IO/CSV/Line_Data_CSV.cpp
        Lines/IO/HDF5/Line_Data_HDF5.hpp
        Lines/IO/HDF5/Line_Data_HDF5.cpp
        Lines/IO/CAPNP/Line_Data_CAPNP.hpp
        Lines/IO/CAPNP/Line_Data_CAPNP.cpp
        Lines/IO/CAPNP/line_data.capnp.h
        Lines/IO/CAPNP/line_data.capnp.c++
        Lines/IO/Binary/Line_Data_Binary.hpp
        Lines/IO/Binary/Line_Data_Binary.cpp
        Lines/IO/JSON/Line_Data_JSON.hpp
        Lines/IO/JSON/Line_Data_JSON.cpp
        #Lines/IO/RocksDB/Line_Data_RocksDB.hpp
        #Lines/IO/RocksDB/Line_Data_RocksDB.cpp
        #Lines/IO/LMDB/Line_Data_LMDB.hpp
        #Lines/IO/LMDB/Line_Data_LMDB.cpp

        Media/Image_Data.cpp
        Media/Image_Data.hpp
        Media/Video_Data.cpp
        Media/Video_Data.hpp
        Media/Media_Data.hpp
        Media/Media_Data.cpp
        Media/HDF5_Data.hpp
        Media/HDF5_Data.cpp
        Media/Video_Data_Loader.hpp

        Points/Point_Data.hpp
        Points/Point_Data.cpp
        Points/IO/CSV/Point_Data_CSV.hpp
        Points/IO/CSV/Point_Data_CSV.cpp
        Points/IO/JSON/Point_Data_JSON.hpp
        Points/IO/JSON/Point_Data_JSON.cpp
        Points/points.hpp
        Points/utils/point_geometry.hpp

        Masks/Mask_Data.hpp
        Masks/Mask_Data.cpp
        Masks/IO/HDF5/Mask_Data_HDF5.hpp
        Masks/IO/HDF5/Mask_Data_HDF5.cpp
        Masks/IO/Image/Mask_Data_Image.hpp
        Masks/IO/Image/Mask_Data_Image.cpp
        Masks/IO/JSON/Mask_Data_JSON.hpp
        Masks/IO/JSON/Mask_Data_JSON.cpp
        Masks/masks.hpp
        Masks/masks.cpp

        AnalogTimeSeries/Analog_Time_Series.hpp
        AnalogTimeSeries/Analog_Time_Series.cpp
        AnalogTimeSeries/IO/Binary/Analog_Time_Series_Binary.hpp
        AnalogTimeSeries/IO/Binary/Analog_Time_Series_Binary.cpp
        AnalogTimeSeries/IO/CSV/Analog_Time_Series_CSV.hpp
        AnalogTimeSeries/IO/CSV/Analog_Time_Series_CSV.cpp
        AnalogTimeSeries/IO/JSON/Analog_Time_Series_JSON.hpp
        AnalogTimeSeries/IO/JSON/Analog_Time_Series_JSON.cpp

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

        utils/armadillo_wrap/analog_armadillo.hpp
        utils/armadillo_wrap/analog_armadillo.cpp
        utils/connected_component.hpp
        utils/connected_component.cpp
        utils/hdf5_mask_load.hpp
        utils/json_helpers.hpp
        Lines/utils/line_resampling.cpp
        Lines/utils/line_resampling.hpp


        loaders/loading_utils.hpp
        loaders/loading_utils.cpp
        utils/mask_operations.cpp
        utils/mask_operations.hpp
        utils/opencv_utility.hpp
        utils/opencv_utility.cpp
        utils/polynomial/polynomial_fit.hpp
        utils/polynomial/polynomial_fit.cpp
        utils/polynomial/parametric_polynomial_utils.cpp
        utils/polynomial/parametric_polynomial_utils.hpp
        utils/ProcessingOptions.hpp
        utils/simplify_line.cpp
        utils/simplify_line.hpp
        utils/skeletonize.cpp
        utils/skeletonize.hpp
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
        transforms/Masks/order_line.cpp
        transforms/Masks/order_line.hpp
        transforms/Masks/mask_skeletonize.cpp
        transforms/Masks/mask_skeletonize.hpp

        LockState/LockState.hpp
        ImageSize/ImageSize.hpp
        Lines/utils/line_geometry.cpp
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
