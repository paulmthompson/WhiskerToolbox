find_package(Qt6 REQUIRED COMPONENTS Widgets Core)
find_package(reflectcpp CONFIG REQUIRED)

add_library(CommandLog_Widget STATIC
    Core/CommandLogStateData.hpp
    Core/CommandLogState.hpp
    Core/CommandLogState.cpp
    UI/CommandLogWidget.hpp
    UI/CommandLogWidget.cpp
    CommandLogWidgetRegistration.hpp
    CommandLogWidgetRegistration.cpp
)

target_link_libraries(CommandLog_Widget
    PUBLIC
        EditorState
    PRIVATE
        Qt6::Widgets
        Qt6::Core
        Commands
        reflectcpp::reflectcpp
)

target_include_directories(CommandLog_Widget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_compiler_warnings(CommandLog_Widget)

set_target_properties(CommandLog_Widget PROPERTIES
    AUTOMOC ON
)

enable_whiskertoolbox_shared_pch(CommandLog_Widget)
