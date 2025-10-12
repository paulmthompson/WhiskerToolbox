find_package(Eigen3 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
#find_package(ortools CONFIG REQUIRED)

set(STATE_ESTIMATION_SOURCES
    StateEstimation.hpp
    StateEstimation.cpp
    Kalman/kalman.hpp
    Kalman/kalman.cpp
    Kalman/KalmanFilter.hpp
    Kalman/KalmanFilter.cpp
    Assignment/hungarian.hpp
    Assignment/hungarian.cpp
    Assignment/AssignmentProblem.hpp
    Assignment/AssignmentProblem.cpp
    Assignment/IAssigner.hpp
    Assignment/HungarianAssigner.hpp
    Assignment/HungarianAssigner.cpp
    Cost/CostFunctions.hpp
    Cost/CostFunctions.cpp
    Features/FeatureVector.hpp
    Features/FeatureVector.cpp
    Features/LineFeatureExtractor.hpp
    Features/LineFeatureExtractor.cpp
    Tracking/MultiFeatureKalman.hpp
    Tracking/MultiFeatureKalman.cpp
    Tracking/TrackingSession.hpp
    Tracking/TrackingSession.cpp
    Filter/IFilter.hpp
    Tracker.hpp
    Tracker.cpp
    Common.hpp
    DataSource.hpp
    MinCostFlowTracker.hpp
    MinCostFlowTracker.cpp
    MinCostFlowSolver.hpp
    MinCostFlowSolver.cpp
)

# Optional demo/example files (not included in library)
set(STATE_ESTIMATION_EXAMPLES
    examples/tracking_framework_demo.cpp
)

add_library(StateEstimation STATIC ${STATE_ESTIMATION_SOURCES})

target_include_directories(StateEstimation PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>  # For Entity/ and TimeFrame/ includes
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_include_directories(StateEstimation PRIVATE
    "$<BUILD_INTERFACE:${ortools_SOURCE_DIR}>"  # For ortools includes
)



target_link_libraries(StateEstimation PUBLIC
    NEURALYZER_GEOMETRY
    WhiskerToolbox::Entity
    WhiskerToolbox::TimeFrame
)

target_link_libraries(StateEstimation PRIVATE Eigen3::Eigen)
target_link_libraries(StateEstimation PRIVATE ortools::ortools)
target_link_libraries(StateEstimation PUBLIC spdlog::spdlog_header_only)


if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(StateEstimation PRIVATE ${CLANG_OPTIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(StateEstimation PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(StateEstimation PRIVATE ${MSVC_WARNINGS})
endif()

set_target_properties(StateEstimation PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

install(FILES ${STATE_ESTIMATION_INCLUDES}
    DESTINATION include/StateEstimation
)
