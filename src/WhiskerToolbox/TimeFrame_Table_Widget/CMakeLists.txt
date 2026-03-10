# TimeFrame_Table_Widget Library CMakeLists.txt

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(TIMEFRAME_TABLE_WIDGET_SOURCES
    TimeFrame_Table_Widget.hpp
    TimeFrame_Table_Widget.cpp
    TimeFrame_Table_Widget.ui
)

add_library(TimeFrame_Table_Widget STATIC ${TIMEFRAME_TABLE_WIDGET_SOURCES})

set_target_properties(TimeFrame_Table_Widget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(TimeFrame_Table_Widget PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
)

target_link_libraries(TimeFrame_Table_Widget PRIVATE DataManager)

target_include_directories(TimeFrame_Table_Widget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_compile_definitions(TimeFrame_Table_Widget PRIVATE _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(TimeFrame_Table_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(TimeFrame_Table_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(TimeFrame_Table_Widget PRIVATE ${MSVC_WARNINGS})
endif()

set_target_properties(TimeFrame_Table_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
enable_whiskertoolbox_shared_pch(TimeFrame_Table_Widget)