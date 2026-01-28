find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(DATA_INSPECTOR_WIDGET_SOURCES
        DataInspectorState.hpp
        DataInspectorState.cpp
        DataInspectorWidgetRegistration.hpp
        DataInspectorWidgetRegistration.cpp
        DataInspectorPropertiesWidget.hpp
        DataInspectorPropertiesWidget.cpp
        DataInspectorPropertiesWidget.ui
        DataInspectorViewWidget.hpp
        DataInspectorViewWidget.cpp
        DataInspectorViewWidget.ui
        )

add_library(DataInspector_Widget STATIC ${DATA_INSPECTOR_WIDGET_SOURCES})

set_target_properties(DataInspector_Widget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(DataInspector_Widget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
)

# Core dependencies
target_link_libraries(DataInspector_Widget PRIVATE DataManager)
target_link_libraries(DataInspector_Widget PRIVATE EditorState)
target_link_libraries(DataInspector_Widget PRIVATE GroupManagementWidget)

# JSON libraries for serialization
target_link_libraries(DataInspector_Widget PRIVATE nlohmann_json::nlohmann_json)

target_include_directories(DataInspector_Widget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" # This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>" # Parent for sibling widgets
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" # For DataManager etc
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(DataInspector_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DataInspector_Widget PRIVATE ${GCC_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(DataInspector_Widget PRIVATE ${MSVC_OPTIONS})
endif()
