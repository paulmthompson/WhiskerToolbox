# IO Plugin System CMakeLists.txt

# Core plugin interface
set(IO_CORE_SOURCES
    DataLoader.hpp
    LoaderRegistry.hpp
    LoaderRegistry.cpp
    LoaderRegistration.hpp
    LoaderRegistration.cpp
    PluginLoader.hpp
    PluginLoader.cpp
    DataFactory.hpp
    PluginDemo.hpp
    loaders/CSVLoader.hpp
    loaders/CSVLoader.cpp
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

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(DataManagerIO PRIVATE ${CLANG_OPTIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DataManagerIO PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(DataManagerIO PRIVATE ${MSVC_WARNINGS})
endif()

target_compile_definitions(DataManagerIO PUBLIC ENABLE_CAPNPROTO)

# Set properties
set_target_properties(DataManagerIO PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
