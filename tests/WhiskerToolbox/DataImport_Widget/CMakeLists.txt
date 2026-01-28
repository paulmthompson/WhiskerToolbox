# Tests for DataImport_Widget

if (APPLE)
    message(STATUS "DataImport_Widget tests currently not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "DataImport_Widget tests currently not supported on Windows")
    return()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Test)
find_package(reflectcpp CONFIG REQUIRED)

# === DataImportWidgetState Unit Tests ===
# These tests need the full DataImport_Widget library which includes DataManager dependency
add_executable(DataImportWidgetState_test
    DataImportWidgetState.test.cpp
)

target_link_libraries(DataImportWidgetState_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Test
        Qt6::Widgets
        Qt6::Core
        DataImport_Widget
        DataManager       # Required by DataImport_Widget
        DataManagerIO     # Required for loader registry
        TensorData        # Required for Tensor imports
)

catch_discover_tests(DataImportWidgetState_test)

# === DataImportTypeRegistry Unit Tests ===
# These tests need the full registry with registered widget types
add_executable(DataImportTypeRegistry_test
    DataImportTypeRegistry.test.cpp
)

target_link_libraries(DataImportTypeRegistry_test
    PRIVATE
        Catch2::Catch2WithMain
        Qt6::Test
        Qt6::Widgets
        Qt6::Core
        DataImport_Widget
        DataManager       # Needed by widget factories
        DataManagerIO     # Required for loader registry
        TensorData        # Required for Tensor imports
)

catch_discover_tests(DataImportTypeRegistry_test)
