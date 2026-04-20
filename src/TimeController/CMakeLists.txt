# TimeController: Qt-free current visualization time and active TimeKey

add_library(TimeController STATIC
    TimeController.hpp
    TimeController.cpp
)

target_include_directories(TimeController PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(TimeController PUBLIC WhiskerToolbox::TimeFrame)

set_target_compiler_warnings(TimeController)

set_target_properties(TimeController PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_library(WhiskerToolbox::TimeController ALIAS TimeController)

enable_whiskertoolbox_shared_pch(TimeController)
