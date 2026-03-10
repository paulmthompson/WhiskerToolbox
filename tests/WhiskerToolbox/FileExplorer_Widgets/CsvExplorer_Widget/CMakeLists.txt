# Tests for CsvExplorer_Widget

if (APPLE)
    message(STATUS "CsvExplorer_Widget tests currently not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "CsvExplorer_Widget tests currently not supported on Windows")
    return()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Test)

# === CsvDatasetPreviewModel Unit Tests ===
add_executable(CsvDatasetPreviewModel_test
    CsvDatasetPreviewModel.test.cpp
)

target_link_libraries(CsvDatasetPreviewModel_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Test
        Qt6::Widgets
        Qt6::Core
        CsvExplorer_Widget
        DataManager
)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(CsvDatasetPreviewModel_test)
else()
    add_test(NAME CsvDatasetPreviewModel_test_all
        COMMAND CsvDatasetPreviewModel_test
        )
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(CsvDatasetPreviewModel_test PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(CsvDatasetPreviewModel_test PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(CsvDatasetPreviewModel_test PRIVATE ${MSVC_WARNINGS})
endif()

enable_whiskertoolbox_shared_test_pch(CsvDatasetPreviewModel_test)