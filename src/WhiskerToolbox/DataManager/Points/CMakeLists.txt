
set(points_sources
    Point_Data.hpp
    Point_Data.cpp
    IO/CSV/Point_Data_CSV.hpp
    IO/CSV/Point_Data_CSV.cpp
    IO/JSON/Point_Data_JSON.hpp
    IO/JSON/Point_Data_JSON.cpp
)

set(points_test_sources
    Point_Data.test.cpp
)

prefix_list_items(points_sources "Points")
set(DATAMANAGER_SOURCES ${DATAMANAGER_SOURCES} ${points_sources} PARENT_SCOPE)

add_tests_to_global(points_test_sources
                    "${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/Points"
                    DATAMANAGER_TEST_SOURCES)
