
find_package(HDF5 COMPONENTS CXX REQUIRED)

add_library(DataManager SHARED
        DataManager.hpp
        DataManager.cpp

        TimeFrame.hpp
        TimeFrame.cpp

        Lines/Line_Data.hpp
        Lines/Line_Data.cpp

        Media/Image_Data.cpp
        Media/Image_Data.hpp
        Media/Video_Data.cpp
        Media/Video_Data.hpp
        Media/Media_Data.hpp
        Media/Media_Data.cpp

        Points/Point_Data.hpp
        Points/Point_Data.cpp

        Masks/Mask_Data.hpp
        Masks/Mask_Data.cpp

        AnalogTimeSeries/Analog_Time_Series.hpp
        AnalogTimeSeries/Analog_Time_Series.cpp

        DigitalTimeSeries/Digital_Interval_Series.hpp
        DigitalTimeSeries/Digital_Interval_Series.cpp

        DigitalTimeSeries/Digital_Event_Series.hpp
        DigitalTimeSeries/Digital_Event_Series.cpp

        loaders/CSV_Loaders.cpp
        loaders/CSV_Loaders.hpp

        utils/container.hpp
        utils/hdf5_mask_load.hpp
        utils/opencv_utility.hpp
        utils/opencv_utility.cpp
        utils/string_manip.hpp
        utils/glob.hpp
        Media/HDF5_Data.hpp
        Media/HDF5_Data.cpp

        transforms/data_transforms.cpp
        transforms/data_transforms.hpp

        loaders/binary_loaders.hpp
        Points/points.hpp
        Masks/masks.hpp
        Lines/lines.hpp
)

target_include_directories(DataManager PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"

        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )


target_link_libraries(DataManager PRIVATE ffmpeg_wrapper::ffmpeg_wrapper)

target_link_libraries(DataManager PRIVATE HighFive)

if (APPLE)
    target_link_libraries(DataManager PRIVATE hdf5::hdf5-static hdf5::hdf5_cpp-static)
else()
    target_link_libraries(DataManager PRIVATE hdf5::hdf5-shared hdf5::hdf5_cpp-shared)
endif()

target_link_libraries(DataManager PRIVATE opencv_core opencv_imgproc opencv_imgcodecs)

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
