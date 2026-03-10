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

# Apply the same compiler flags as the main project
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(GroupContextMenu PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(GroupContextMenu PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(GroupContextMenu PRIVATE ${MSVC_WARNINGS})
endif()

enable_whiskertoolbox_shared_pch(GroupContextMenu)