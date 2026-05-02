find_package(spdlog CONFIG REQUIRED)

add_library(ObserverData STATIC
    Observer_Data.hpp
    Observer_Data.cpp
)

target_include_directories(ObserverData PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"  # For "Observer/..." includes
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(ObserverData PUBLIC spdlog::spdlog_header_only)

set_target_compiler_warnings(ObserverData)

set_target_properties(ObserverData PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_library(WhiskerToolbox::ObserverData ALIAS ObserverData)

enable_whiskertoolbox_shared_pch(ObserverData)
