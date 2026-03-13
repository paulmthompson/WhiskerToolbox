find_package(nlohmann_json CONFIG REQUIRED)
find_package(reflectcpp CONFIG REQUIRED)

add_library(CoreUtilities STATIC
    include/CoreUtilities/string_manip.hpp
    include/CoreUtilities/color.hpp
    src/color.cpp
)

target_include_directories(CoreUtilities PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)

target_link_libraries(CoreUtilities PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(CoreUtilities PUBLIC reflectcpp::reflectcpp)

set_target_compiler_warnings(CoreUtilities)

set_target_properties(CoreUtilities PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)