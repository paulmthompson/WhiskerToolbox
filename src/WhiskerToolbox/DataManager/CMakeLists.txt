
find_package(HDF5 COMPONENTS CXX REQUIRED)

set(CMAKE_CUDA_FLAGS "-allow-unsupported-compiler")

find_package(Torch REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

find_package(nlohmann_json CONFIG REQUIRED)

#find_package(unofficial-lmdb CONFIG REQUIRED)
find_package(CapnProto CONFIG REQUIRED)
#find_package(RocksDB CONFIG REQUIRED)

find_package(LAPACK REQUIRED)

add_library(DataManager SHARED
        DataManager.hpp
        DataManager.cpp

        TimeFrame.hpp
        TimeFrame.cpp

        Lines/Line_Data.hpp
        Lines/Line_Data.cpp
        Lines/lines.hpp
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
        Points/points.hpp

        Masks/Mask_Data.hpp
        Masks/Mask_Data.cpp
        Masks/IO/HDF5/Mask_Data_HDF5.hpp
        Masks/IO/HDF5/Mask_Data_HDF5.cpp
        Masks/masks.hpp

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
        DigitalTimeSeries/IO/JSON/Digital_Interval_Series_JSON.hpp
        DigitalTimeSeries/IO/JSON/Digital_Interval_Series_JSON.cpp
        DigitalTimeSeries/interval_data.hpp

        DigitalTimeSeries/Digital_Event_Series.hpp
        DigitalTimeSeries/Digital_Event_Series.cpp
        DigitalTimeSeries/IO/CSV/Digital_Event_Series_Loader.cpp
        DigitalTimeSeries/IO/CSV/Digital_Event_Series_Loader.hpp

        Tensors/Tensor_Data.hpp
        Tensors/Tensor_Data.cpp

        loaders/CSV_Loaders.cpp
        loaders/CSV_Loaders.hpp

        utils/container.hpp
        utils/hdf5_mask_load.hpp
        utils/opencv_utility.hpp
        utils/opencv_utility.cpp
        utils/string_manip.hpp
        utils/json_helpers.hpp
        utils/armadillo_wrap/analog_armadillo.hpp
        utils/polynomial/polynomial_fit.hpp
        utils/polynomial/polynomial_fit.cpp

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
        transforms/Lines/line_angle.hpp
        transforms/Lines/line_angle.cpp
        transforms/Lines/line_min_point_dist.hpp
        transforms/Lines/line_min_point_dist.cpp
        transforms/Masks/mask_area.cpp
        transforms/Masks/mask_area.hpp
        transforms/Masks/mask_to_line.cpp
        transforms/Masks/mask_to_line.hpp
        utils/connected_component.hpp
        utils/connected_component.cpp
        utils/mask_operations.cpp
        utils/mask_operations.hpp
        transforms/Masks/order_line.cpp
        transforms/Masks/order_line.hpp
        utils/simplify_line.cpp
        utils/simplify_line.hpp
        utils/skeletonize.cpp
        utils/skeletonize.hpp

        loaders/binary_loaders.hpp
        loaders/hdf5_loaders.hpp
        loaders/hdf5_loaders.cpp

        LockState/LockState.hpp
        Observer/Observer_Data.hpp
        Observer/Observer_Data.cpp
        ImageSize/ImageSize.hpp
        utils/armadillo_wrap/analog_armadillo.cpp
        DataManagerTypes.hpp
        loaders/loading_utils.hpp
        loaders/loading_utils.cpp



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

target_link_libraries(DataManager PRIVATE opencv_core opencv_imgproc opencv_imgcodecs)

target_link_libraries(DataManager PRIVATE armadillo)
target_link_libraries(DataManager PRIVATE LAPACK::LAPACK)

target_link_libraries(DataManager PUBLIC nlohmann_json::nlohmann_json)
#target_link_libraries(DataManager PRIVATE unofficial::lmdb::lmdb)
target_link_libraries(DataManager PRIVATE CapnProto::capnp)
#target_link_libraries(DataManager PRIVATE RocksDB::rocksdb RocksDB::rocksdb-shared)

target_link_libraries(DataManager PRIVATE "${TORCH_LIBRARIES}")

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
