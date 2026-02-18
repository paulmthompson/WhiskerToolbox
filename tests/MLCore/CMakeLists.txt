if (APPLE)
    message(STATUS "Testing Currenly not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing Currenly not supported on Windows")
    return()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

add_executable(test_MLCore
    ClassBalancing.test.cpp
    ClassificationMetrics.test.cpp
    FeatureConverter.test.cpp
    FeatureValidator.test.cpp
    DBSCANOperation.test.cpp
    KMeansOperation.test.cpp
    LabelAssembler.test.cpp
    LogisticRegressionOperation.test.cpp
    MLModelOperation.test.cpp
    NaiveBayesOperation.test.cpp
    PredictionWriter.test.cpp
    RandomForestOperation.test.cpp
    TimeEntityUtils.test.cpp
)

target_link_libraries(test_MLCore PRIVATE Catch2::Catch2WithMain MLCore)
target_link_libraries(test_MLCore PRIVATE DataManager TensorData)

target_include_directories(test_MLCore PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/MLCore>"
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/>"
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/tests/MLCore>"
)

if (ENABLE_DETAILED_TEST_DISCOVERY)
    if (WIN32)
        catch_discover_tests(test_MLCore
                DL_PATHS
                "$ENV{PATH}"
                "${CMAKE_BINARY_DIR}"
        )
    else()
        catch_discover_tests(test_MLCore)
    endif()
else()
    add_test(NAME test_MLCore_all
        COMMAND test_MLCore
        )
endif()