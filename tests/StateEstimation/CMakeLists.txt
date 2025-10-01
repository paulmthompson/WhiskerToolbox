if (APPLE)
    message(STATUS "Testing Currenly not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing Currenly not supported on Windows")
    return()
endif()

find_package(Eigen3 CONFIG REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

add_executable(test_StateEstimation
     ${CMAKE_SOURCE_DIR}/src/StateEstimation/Tracking/MultiFeatureKalman.test.cpp
     ${CMAKE_SOURCE_DIR}/src/StateEstimation/Features/FeatureVector.test.cpp
     ${CMAKE_SOURCE_DIR}/src/StateEstimation/Assignment/AssignmentProblem.test.cpp
     ${CMAKE_SOURCE_DIR}/src/StateEstimation/Features/LineFeatureExtractor.test.cpp
     ${CMAKE_SOURCE_DIR}/src/StateEstimation/Kalman/KalmanFilter.test.cpp
)

target_link_libraries(test_StateEstimation PRIVATE 
Catch2::Catch2WithMain 
NEURALYZER_GEOMETRY
StateEstimation
Eigen3::Eigen
)

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
