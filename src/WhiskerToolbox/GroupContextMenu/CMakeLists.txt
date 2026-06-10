add_library(GroupContextMenu STATIC
    GroupContextMenuHandler.cpp
    GroupContextMenuHandler.hpp
)

set_target_properties(GroupContextMenu PROPERTIES
    AUTOUIC ON
    AUTOMOC ON
)

target_link_libraries(GroupContextMenu
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        Entity
        GroupManagementWidget
)

target_include_directories(GroupContextMenu
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

set_target_compiler_warnings(GroupContextMenu)

enable_whiskertoolbox_shared_pch(GroupContextMenu)