
add_library(DataTypeEnum INTERFACE)

target_include_directories(DataTypeEnum INTERFACE 
"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
"$<INSTALL_INTERFACE:include>")