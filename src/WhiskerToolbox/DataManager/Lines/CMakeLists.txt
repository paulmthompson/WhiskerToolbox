
set(lines_subdirectory_sources
        Line_Data.hpp
        Line_Data.cpp
        lines.hpp
        lines.cpp
        utils/line_geometry.hpp
        utils/line_geometry.cpp
        utils/line_resampling.cpp
        utils/line_resampling.hpp
        IO/CSV/Line_Data_CSV.hpp
        IO/CSV/Line_Data_CSV.cpp
        IO/HDF5/Line_Data_HDF5.hpp
        IO/HDF5/Line_Data_HDF5.cpp
        IO/CAPNP/Line_Data_CAPNP.hpp
        IO/CAPNP/Line_Data_CAPNP.cpp
        IO/CAPNP/line_data.capnp.h
        IO/CAPNP/line_data.capnp.c++
        IO/Binary/Line_Data_Binary.hpp
        IO/Binary/Line_Data_Binary.cpp
        IO/JSON/Line_Data_JSON.hpp
        IO/JSON/Line_Data_JSON.cpp
        #Lines/IO/RocksDB/Line_Data_RocksDB.hpp
        #Lines/IO/RocksDB/Line_Data_RocksDB.cpp
        #Lines/IO/LMDB/Line_Data_LMDB.hpp
        #Lines/IO/LMDB/Line_Data_LMDB.cpp
)

set(lines_test_sources
        Line_Data.test.cpp
        utils/line_geometry.test.cpp
)

# Prepare a list of sources with paths relative to the PARENT CMakeLists.txt
set(sources_to_add_to_parent "")
foreach(source_file ${lines_subdirectory_sources})
    # Prepend "Lines/" to make paths relative to the parent DataManager/CMakeLists.txt
    list(APPEND sources_to_add_to_parent "Lines/${source_file}")
endforeach()

set(DATAMANAGER_SOURCES ${DATAMANAGER_SOURCES} ${sources_to_add_to_parent} PARENT_SCOPE)



set(test_to_add_to_parent "")
foreach(source_file ${lines_test_sources})
    list(APPEND test_to_add_to_parent "${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/Lines/${source_file}")
endforeach()

get_property(DATA_MANAGER_TEST_SOURCES_TEMP GLOBAL PROPERTY DATAMANAGER_TEST_SOURCES)
list(APPEND DATAMANAGER_TEST_SOURCES_TEMP ${test_to_add_to_parent})

set_property(GLOBAL PROPERTY DATAMANAGER_TEST_SOURCES "${DATAMANAGER_TEST_SOURCES_TEMP}")