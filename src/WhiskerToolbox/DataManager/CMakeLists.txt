
find_package(HDF5 COMPONENTS CXX REQUIRED)

add_library(DataManager SHARED
        DataManager.hpp
        DataManager.cpp
        TimeFrame.hpp
        TimeFrame.cpp

        Media/Image_Data.cpp
        Media/Image_Data.hpp
        Media/Video_Data.cpp
        Media/Video_Data.hpp
        Media/Media_Data.hpp
        Media/Media_Data.cpp
        Lines/Line_Data.hpp
        Lines/Line_Data.cpp
        utils/hdf5_mask_load.hpp
        Masks/Mask_Data.hpp
        Points/Point_Data.hpp
        Masks/Mask_Data.cpp
        utils/container.hpp
        utils/opencv_utility.hpp
        utils/opencv_utility.cpp
        utils/string_manip.hpp
        Points/Point_Data.cpp
)

target_include_directories(DataManager PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    )

target_link_libraries(DataManager PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
#target_link_libraries(DataManager PRIVATE Qt${QT_VERSION_MAJOR}::Multimedia)

target_link_libraries(DataManager PRIVATE ffmpeg_wrapper::ffmpeg_wrapper)

target_link_libraries(DataManager PRIVATE HighFive)
#target_link_libraries(DataManager PRIVATE HDF5::HDF5)
target_link_libraries(DataManager PRIVATE hdf5::hdf5-shared hdf5::hdf5_cpp-shared hdf5::hdf5_hl_cpp-shared)

target_link_libraries(DataManager PRIVATE opencv_core opencv_imgproc opencv_imgcodecs)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(DataManager PRIVATE ${CLANG_OPTIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
    #set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DataManager PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(DataManager PRIVATE ${MSVC_WARNINGS})
endif()
