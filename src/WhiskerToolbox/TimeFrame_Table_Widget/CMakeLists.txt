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

target_link_libraries(TimeFrame_Table_Widget PUBLIC
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

set_target_compiler_warnings(TimeFrame_Table_Widget)

set_target_properties(TimeFrame_Table_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
enable_whiskertoolbox_shared_pch(TimeFrame_Table_Widget)