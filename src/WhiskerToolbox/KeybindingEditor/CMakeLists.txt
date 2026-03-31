find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_library(KeybindingEditor STATIC
    KeybindingEditorRegistration.hpp
    KeybindingEditorRegistration.cpp
    KeybindingEditorState.hpp
    KeybindingEditorState.cpp
    KeybindingEditor.hpp
    KeybindingEditor.cpp
)

target_link_libraries(KeybindingEditor
    PUBLIC
        Qt6::Core
        Qt6::Widgets
)

target_link_libraries(KeybindingEditor PUBLIC EditorState)
target_link_libraries(KeybindingEditor PRIVATE KeymapSystem)

target_include_directories(KeybindingEditor PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_properties(KeybindingEditor PROPERTIES
    AUTOMOC ON
)

set_target_compiler_warnings(KeybindingEditor)
