find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(TOOLTIP_MANAGER_SOURCES

        PlotTooltipManager.cpp
        PlotTooltipManager.hpp

)

add_library(TooltipManager STATIC ${TOOLTIP_MANAGER_SOURCES})

set_target_properties(TooltipManager PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(TooltipManager PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
)

target_include_directories(TooltipManager PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(TooltipManager PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(TooltipManager PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(TooltipManager PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(TooltipManager PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
enable_whiskertoolbox_shared_pch(TooltipManager)