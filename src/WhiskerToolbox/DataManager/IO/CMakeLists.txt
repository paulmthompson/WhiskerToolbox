# IO Plugin System CMakeLists.txt

# Core plugin interface
set(IO_CORE_SOURCES
    DataLoader.hpp
    LoaderRegistry.hpp
    LoaderRegistry.cpp
    PluginLoader.hpp
    PluginLoader.cpp
    DataFactory.hpp
    PluginDemo.hpp
)

# Create the core IO library
add_library(DataManagerIO ${IO_CORE_SOURCES})

target_include_directories(DataManagerIO PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(DataManagerIO PUBLIC
    nlohmann_json::nlohmann_json
    NEURALYZER_GEOMETRY
)

# Add subdirectories for specific format plugins
if(ENABLE_HDF5)
    add_subdirectory(HDF5)
endif()

if(ENABLE_CAPNPROTO)
    add_subdirectory(CapnProto)
endif()

# Set properties
set_target_properties(DataManagerIO PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
