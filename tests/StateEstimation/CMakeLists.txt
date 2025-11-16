if (APPLE)
    message(STATUS "Testing Currenly not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing Currenly not supported on Windows")
    return()
endif()

find_package(Eigen3 CONFIG REQUIRED)
if (ENABLE_ORTOOLS)
    find_package(Boost COMPONENTS random serialization REQUIRED)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

set(STATE_ESTIMATION_TEST_SOURCES
     ${CMAKE_SOURCE_DIR}/src/StateEstimation/Tracking/MultiFeatureKalman.test.cpp
     ${CMAKE_SOURCE_DIR}/src/StateEstimation/Features/FeatureVector.test.cpp
     ${CMAKE_SOURCE_DIR}/src/StateEstimation/Assignment/AssignmentProblem.test.cpp
     ${CMAKE_SOURCE_DIR}/src/StateEstimation/Features/LineFeatureExtractor.test.cpp
     ${CMAKE_SOURCE_DIR}/src/StateEstimation/Cost/CostFunctions.test.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/test_composite_feature_extractor.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/MaskParticleFilter.test.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/TestFixture.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/TestFixture.test.cpp
)

if(ENABLE_ORTOOLS)
    list(APPEND STATE_ESTIMATION_TEST_SOURCES
        ${CMAKE_SOURCE_DIR}/src/StateEstimation/MinCostFlowTracker.test.cpp
        ${CMAKE_SOURCE_DIR}/src/StateEstimation/Filter/Kalman/KalmanFilter.test.cpp
    )
endif()

add_executable(test_StateEstimation ${STATE_ESTIMATION_TEST_SOURCES})

target_link_libraries(test_StateEstimation PRIVATE 
Catch2::Catch2WithMain 
NEURALYZER_GEOMETRY
StateEstimation
Eigen3::Eigen
)

if(ENABLE_ORTOOLS)
    target_link_libraries(test_StateEstimation PRIVATE ortools::ortools Boost::random Boost::serialization)
endif()

target_include_directories(test_StateEstimation PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/StateEstimation>"
)

if (APPLE)
    set_target_properties(test_StateEstimation PROPERTIES
            BUILD_RPATH "${CMAKE_BINARY_DIR};${CMAKE_BINARY_DIR}/_deps/catch2-build/src"
            INSTALL_RPATH "@executable_path/../Frameworks"
    )
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

endif()

if (WIN32)
    catch_discover_tests(test_StateEstimation
            DL_PATHS
            "$ENV{PATH}"
            "${CMAKE_BINARY_DIR}"
    )
else()
    catch_discover_tests(test_StateEstimation)
endif()
