add_executable(benchmark_analog_event_threshold analog_event_threshold.benchmark.cpp)

target_link_libraries(benchmark_analog_event_threshold
    PRIVATE
    Catch2::Catch2WithMain
    DataManager
)

target_include_directories(benchmark_analog_event_threshold
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
