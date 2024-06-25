
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

        utils/container.hpp
        utils/hdf5_mask_load.hpp
        utils/opencv_utility.hpp
        utils/opencv_utility.cpp
        utils/string_manip.hpp
)

target_include_directories(DataManager PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"

        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )

target_link_libraries(DataManager PRIVATE Qt6::Widgets)

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

install(TARGETS DataManager
        BUNDLE DESTINATION .
        LIBRARY
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(TARGETS DataManager RUNTIME_DEPENDENCY_SET appDeps)

IF (WIN32)

    install(RUNTIME_DEPENDENCY_SET appDeps
            PRE_EXCLUDE_REGEXES
            [[api-ms-win-.*]] [[ext-ms-.*]] [[kernel32\.dll]]
            [[bcrypt.dll]] [[mfplat.dll]] [[msvcrt.dll]] [[ole32.dll]] [[secur32.dll]] [[user32.dll]] [[vcruntime140.dll]]
            [[ws2_32.dll]]
            [[libgcc_s_seh-1\.dll]] [[libstdc\+\+\-6.dll]]
            POST_EXCLUDE_REGEXES
            [[.*/system32/.*\.dll]]
    )

ELSE()

    install(RUNTIME_DEPENDENCY_SET appDeps
            PRE_EXCLUDE_REGEXES
            [[libc\.so\..*]] [[libgcc_s\.so\..*]] [[libm\.so\..*]] [[libstdc\+\+\.so\..*]]
            [[ld.*]] [[libbz2.*]] [[libdl.*]] [[libgmp.*]] [[libgnutls.*]] [[libhogweed.*]]
            [[libpthread.*]] [[librt.*]] [[libz.*]] [[libQt6Widgets.*]]
            POST_EXCLUDE_REGEXES
            [[/lib/x86_64-linux-gnu/*]]
    )

ENDIF()
