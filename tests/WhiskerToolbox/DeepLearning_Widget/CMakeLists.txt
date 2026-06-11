if (APPLE)
    message(STATUS "Testing Currently not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing Currently not supported on Windows")
    return()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Test)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

add_executable(test_dl_widgets
    binding_data/BindingData.test.cpp
    QtTestApp.cpp
    DataSourceComboHelper.test.cpp
    DynamicInputSlotWidget.test.cpp
    OutputSlotWidget.test.cpp
    OutputPipelineWidget.test.cpp
    PostEncoderWidget.test.cpp
    EncoderShapeWidget.test.cpp
    RecurrentBindingWidget.test.cpp
    StaticInputSlotWidget.test.cpp
    SequenceSlotWidget.test.cpp
    ConstraintEnforcer.test.cpp
)

target_link_libraries(test_dl_widgets PRIVATE
    Catch2::Catch2WithMain
    DeepLearning_Widget
    DataManager
    WhiskerToolbox::PointData
    WhiskerToolbox::LineData
    WhiskerToolbox::MaskData
    WhiskerToolbox::MediaData
    Qt6::Widgets
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_dl_widgets PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DataManager>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DeepLearning>"
)

set_target_compiler_warnings(test_dl_widgets)

if (ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_dl_widgets
        PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_dl_widgets_all
        COMMAND test_dl_widgets
    )
    set_property(TEST test_dl_widgets_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

enable_whiskertoolbox_shared_test_pch(test_dl_widgets)
