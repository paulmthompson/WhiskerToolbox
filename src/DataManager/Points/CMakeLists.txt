
set(points_sources
    Point_Data.hpp
    Point_Data.cpp
    utils/Point_Data_utils.hpp
    utils/Point_Data_utils.cpp
    IO/CSV/Point_Data_CSV.hpp
    IO/CSV/Point_Data_CSV.cpp
    IO/JSON/Point_Data_JSON.hpp
    IO/JSON/Point_Data_JSON.cpp
)

# Create the PointData static library
add_library(PointData STATIC ${points_sources})


target_include_directories(PointData PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>  # For DataManager/utils and other utils
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>     # For IO subdirectories
    $<INSTALL_INTERFACE:include>
)

# Link to required libraries
target_link_libraries(PointData PUBLIC
    NEURALYZER_GEOMETRY      # CoreGeometry
    WhiskerToolbox::ObserverData  # Observer
    TimeFrame                # TimeFrame  
    Entity                   # Entity
    nlohmann_json::nlohmann_json  # For JSON support
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(PointData PRIVATE ${CLANG_OPTIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(PointData PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(PointData PRIVATE ${MSVC_WARNINGS})
endif()

# Create alias for consistent naming
add_library(WhiskerToolbox::PointData ALIAS PointData)

set(points_test_sources
    Point_Data.test.cpp
)

add_tests_to_global(points_test_sources
                    "${CMAKE_SOURCE_DIR}/src/DataManager/Points"
                    DATAMANAGER_TEST_SOURCES)
