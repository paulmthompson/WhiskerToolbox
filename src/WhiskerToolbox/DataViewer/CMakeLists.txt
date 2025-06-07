
find_package(glm CONFIG REQUIRED)

set(DATAVIEWER_SOURCES
    XAxis.hpp
    VerticalSpaceManager.cpp
    VerticalSpaceManager.hpp)

add_library(DataViewer SHARED ${DATAVIEWER_SOURCES}
    )

target_include_directories(DataViewer PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(DataViewer PRIVATE ${CLANG_OPTIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DataViewer PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(DataViewer PRIVATE ${MSVC_WARNINGS})
endif()

set_target_properties(DataViewer PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
