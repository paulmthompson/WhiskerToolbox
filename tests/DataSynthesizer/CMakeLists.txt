if (APPLE)
    message(STATUS "Testing currently not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing currently not supported on Windows")
    return()
endif()

add_executable(test_DataSynthesizer
    GeneratorRegistry.test.cpp
    SineWaveGenerator.test.cpp
    SquareWaveGenerator.test.cpp
    TriangleWaveGenerator.test.cpp
    SawtoothWaveGenerator.test.cpp
    GaussianNoiseGenerator.test.cpp
    UniformNoiseGenerator.test.cpp
    PoissonEventsGenerator.test.cpp
    RegularEventsGenerator.test.cpp
    BurstEventsGenerator.test.cpp
    InhomogeneousPoissonEventsGenerator.test.cpp
    RegularIntervalsGenerator.test.cpp
    RandomIntervalsGenerator.test.cpp
    CircleMaskGenerator.test.cpp
    RectangleMaskGenerator.test.cpp
    EllipseMaskGenerator.test.cpp
    GridPointsGenerator.test.cpp
    RandomPointsGenerator.test.cpp
    StraightLineGenerator.test.cpp
    Trajectory.test.cpp
    MovingPointGenerator.test.cpp
    MovingMaskGenerator.test.cpp
    MovingLineGenerator.test.cpp
)

target_include_directories(test_DataSynthesizer PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
)

target_link_libraries(test_DataSynthesizer PRIVATE Catch2::Catch2WithMain)
target_link_libraries(test_DataSynthesizer PRIVATE DataManager)
target_link_libraries(test_DataSynthesizer PRIVATE WhiskerToolbox::AnalogTimeSeries)
target_link_libraries(test_DataSynthesizer PRIVATE WhiskerToolbox::DigitalTimeSeries)
target_link_libraries(test_DataSynthesizer PRIVATE WhiskerToolbox::MaskData)
target_link_libraries(test_DataSynthesizer PRIVATE WhiskerToolbox::PointData)
target_link_libraries(test_DataSynthesizer PRIVATE WhiskerToolbox::LineData)

# Force whole-archive linking for DataSynthesizer to ensure static generator registration
# (each generator .cpp has a file-scope RAII object that registers into GeneratorRegistry,
# but the linker would strip those translation units since tests have no direct symbol references)
if(APPLE)
    target_link_libraries(test_DataSynthesizer PRIVATE -Wl,-force_load,$<TARGET_FILE:DataSynthesizer>)
elseif(MSVC)
    target_link_options(test_DataSynthesizer PRIVATE /WHOLEARCHIVE:DataSynthesizer)
    target_link_libraries(test_DataSynthesizer PRIVATE DataSynthesizer)
else()
    # Linux/GCC/Clang
    target_link_libraries(test_DataSynthesizer PRIVATE -Wl,--whole-archive DataSynthesizer -Wl,--no-whole-archive)
endif()

if (ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_DataSynthesizer)
else()
    add_test(NAME test_DataSynthesizer_all
        COMMAND test_DataSynthesizer
    )
endif()
