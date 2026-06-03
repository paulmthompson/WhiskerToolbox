

add_library(TransformTypes INTERFACE)

target_include_directories(TransformTypes INTERFACE 
"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
"$<INSTALL_INTERFACE:include>")

target_link_libraries(TransformTypes INTERFACE NEURALYZER_GEOMETRY)
target_link_libraries(TransformTypes INTERFACE WhiskerToolbox::TimeFrame)