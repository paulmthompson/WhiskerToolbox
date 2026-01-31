# IO Plugin System CMakeLists.txt

# Core plugin interface
set(IO_CORE_SOURCES
    core/DataLoader.hpp

    core/LoaderRegistry.hpp
    core/LoaderRegistry.cpp
    core/LoaderRegistration.hpp
    core/LoaderRegistration.cpp

    formats/CSV/CSVLoader.hpp
    formats/CSV/CSVLoader.cpp
)

# Create the core IO library
add_library(DataManagerIO SHARED ${IO_CORE_SOURCES})

target_include_directories(DataManagerIO PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(DataManagerIO PUBLIC
    nlohmann_json::nlohmann_json
    NEURALYZER_GEOMETRY
    LineData # Needed to populate CSV line data
)

# Add subdirectories for specific format plugins
if(ENABLE_HDF5)
    add_subdirectory(formats/HDF5)
    target_compile_definitions(DataManagerIO PUBLIC ENABLE_HDF5)
    target_link_libraries(DataManagerIO PRIVATE DataManagerHDF5)
endif()

if(ENABLE_CAPNPROTO)
    add_subdirectory(formats/CapnProto)
    target_compile_definitions(DataManagerIO PUBLIC ENABLE_CAPNPROTO)
    target_link_libraries(DataManagerIO PRIVATE DataManagerIO_CapnProto)
endif()

if(ENABLE_OPENCV)
    add_subdirectory(formats/OpenCV)
    target_compile_definitions(DataManagerIO PUBLIC ENABLE_OPENCV)
    target_link_libraries(DataManagerIO PRIVATE DataManagerOpenCV)
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

# Conditionally define preprocessor macros based on actual library availability
if(ENABLE_CAPNPROTO)

endif()

if(ENABLE_HDF5)

endif()

if(ENABLE_OPENCV)

endif()


# Set properties
set_target_properties(DataManagerIO PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
