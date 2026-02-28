# Test_Widget Library
# Proof-of-concept widget demonstrating the View/Properties split pattern.
# Extracted as a static library so fuzz tests can link against TestWidgetState
# without pulling in the entire WhiskerToolbox executable.
#
# Dependencies:
# - Qt6: Widgets, Core, Gui
# - EditorState: For state management
# - DataManager: For DataManager shared_ptr in state constructor
# - reflectcpp: For JSON serialization (TestWidgetStateData)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)
find_package(reflectcpp CONFIG REQUIRED)

set(TEST_WIDGET_SOURCES
    TestWidgetStateData.hpp
    TestWidgetState.hpp
    TestWidgetState.cpp
    TestWidgetView.hpp
    TestWidgetView.cpp
    TestWidgetProperties.hpp
    TestWidgetProperties.cpp
    TestWidgetRegistration.hpp
    TestWidgetRegistration.cpp
    Test_Widget.hpp
    Test_Widget.cpp
    Test_Widget.ui
)

add_library(Test_Widget STATIC ${TEST_WIDGET_SOURCES})

set_target_properties(Test_Widget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(Test_Widget PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    reflectcpp::reflectcpp

    DataManager
    EditorState
)

target_include_directories(Test_Widget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(Test_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(Test_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(Test_Widget PRIVATE ${MSVC_WARNINGS})
endif()

set_target_properties(Test_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

add_library(WhiskerToolbox::Test_Widget ALIAS Test_Widget)
