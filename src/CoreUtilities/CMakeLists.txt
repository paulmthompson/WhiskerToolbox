

add_library(CoreUtilities STATIC
    include/CoreUtilities/string_manip.hpp
    include/CoreUtilities/color.hpp
    src/color.cpp
)

target_include_directories(CoreUtilities PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)

set_target_compiler_warnings(CoreUtilities)

set_target_properties(CoreUtilities PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)