
find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

set(GROUPMANAGEMENTWIDGET_SOURCES
    GroupManagementWidget.hpp
    GroupManagementWidget.cpp
    GroupManagementWidget.ui
    GroupManager.hpp
    GroupManager.cpp
)

add_library(GroupManagementWidget STATIC ${GROUPMANAGEMENTWIDGET_SOURCES})

set_target_properties(GroupManagementWidget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(GroupManagementWidget PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui

    DataManager
)

# Include directories
target_include_directories(GroupManagementWidget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" # For access to DataManager
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)


if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(GroupManagementWidget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(GroupManagementWidget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(GroupManagementWidget PRIVATE ${MSVC_WARNINGS})
endif()

set_target_properties(GroupManagementWidget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)