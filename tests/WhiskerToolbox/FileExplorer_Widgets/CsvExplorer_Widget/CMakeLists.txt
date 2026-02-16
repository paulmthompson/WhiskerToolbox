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

catch_discover_tests(CsvDatasetPreviewModel_test)
