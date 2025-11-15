add_library(GroupContextMenu STATIC
    GroupContextMenuHandler.cpp
    GroupContextMenuHandler.hpp
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
