find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

qt_add_library(KeymapSystem STATIC
    KeyAction.hpp
    Keymap.hpp
    KeyActionAdapter.hpp
    KeyActionAdapter.cpp
    KeymapManager.hpp
    KeymapManager.cpp
)

set_target_properties(KeymapSystem PROPERTIES
    AUTOMOC ON
)

target_link_libraries(KeymapSystem
    PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets
    PUBLIC EditorState
)

target_include_directories(KeymapSystem
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>"
)

add_library(WhiskerToolbox::KeymapSystem ALIAS KeymapSystem)
set_target_compiler_warnings(KeymapSystem)
enable_whiskertoolbox_shared_pch(KeymapSystem)
