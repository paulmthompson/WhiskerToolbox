
set(lines_subdirectory_sources
        Line_Data.hpp
        Line_Data.cpp
        IO/CSV/Line_Data_CSV.hpp
        IO/CSV/Line_Data_CSV.cpp
        IO/HDF5/Line_Data_HDF5.hpp
        IO/HDF5/Line_Data_HDF5.cpp
        IO/JSON/Line_Data_JSON.hpp
        IO/JSON/Line_Data_JSON.cpp
        #Lines/IO/RocksDB/Line_Data_RocksDB.hpp
        #Lines/IO/RocksDB/Line_Data_RocksDB.cpp
        #Lines/IO/LMDB/Line_Data_LMDB.hpp
        #Lines/IO/LMDB/Line_Data_LMDB.cpp
)

set(lines_test_sources
        Line_Data.test.cpp
)

prefix_list_items(lines_subdirectory_sources "Lines")
set(DATAMANAGER_SOURCES ${DATAMANAGER_SOURCES} ${lines_subdirectory_sources} PARENT_SCOPE)

add_tests_to_global(lines_test_sources 
                    "${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/Lines" 
                    DATAMANAGER_TEST_SOURCES)