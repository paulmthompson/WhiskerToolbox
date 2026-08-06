find_package(spdlog CONFIG REQUIRED)

set(PLOT_ZOOM_PROFILE_SOURCES
    PlotZoomProfile.cpp
    PlotZoomProfile.hpp
)

add_library(PlotZoomProfile STATIC ${PLOT_ZOOM_PROFILE_SOURCES})

set_target_properties(PlotZoomProfile PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(PlotZoomProfile PUBLIC spdlog::spdlog_header_only)

target_include_directories(PlotZoomProfile PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_compiler_warnings(PlotZoomProfile)
