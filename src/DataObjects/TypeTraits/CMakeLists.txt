
add_library(DataTypeTraits INTERFACE)

target_include_directories(DataTypeTraits INTERFACE 
"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
"$<INSTALL_INTERFACE:include>")