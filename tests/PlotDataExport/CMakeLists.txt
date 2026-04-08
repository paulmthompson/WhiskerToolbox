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

add_executable(test_PlotDataExport
        test_RasterCSVExport.cpp
        test_HistogramCSVExport.cpp
        test_HeatmapCSVExport.cpp
)

target_include_directories(test_PlotDataExport PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
)

target_link_libraries(test_PlotDataExport PRIVATE Catch2::Catch2WithMain)
target_link_libraries(test_PlotDataExport PRIVATE PlotDataExport DataManager)

# Force whole-archive linking for TransformsV2 to ensure static transform registration.
# PlotDataExport → GatherResult → TransformsV2 (RAII registered transforms); without
# whole-archive, the linker strips the registration objects since tests have no direct
# symbol references.
if(APPLE)
    target_link_libraries(test_PlotDataExport PRIVATE -Wl,-force_load,$<TARGET_FILE:TransformsV2>)
elseif(MSVC)
    target_link_options(test_PlotDataExport PRIVATE /WHOLEARCHIVE:TransformsV2)
    target_link_libraries(test_PlotDataExport PRIVATE TransformsV2)
else()
    # Linux / GCC / Clang
    target_link_libraries(test_PlotDataExport PRIVATE
            -Wl,--whole-archive TransformsV2 -Wl,--no-whole-archive)
endif()

if (ENABLE_DETAILED_TEST_DISCOVERY)
    if (WIN32)
        catch_discover_tests(test_PlotDataExport
                DL_PATHS
                "$ENV{PATH}"
                "${CMAKE_BINARY_DIR}"
        )
    else()
        catch_discover_tests(test_PlotDataExport)
    endif()
else()
    add_test(NAME test_PlotDataExport_all
            COMMAND test_PlotDataExport
    )
endif()
