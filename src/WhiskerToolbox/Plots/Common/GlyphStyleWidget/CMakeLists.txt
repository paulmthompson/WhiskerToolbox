find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(GLYPH_STYLE_WIDGET_SOURCES

        Core/GlyphStyleState.cpp
        Core/GlyphStyleState.hpp
        GlyphStyleControls.cpp
        GlyphStyleControls.hpp

)

add_library(GlyphStyleWidget STATIC ${GLYPH_STYLE_WIDGET_SOURCES})

set_target_properties(GlyphStyleWidget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(GlyphStyleWidget PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
)

target_link_libraries(GlyphStyleWidget PUBLIC CorePlotting)

target_include_directories(GlyphStyleWidget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For Plots/Common sibling access
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(GlyphStyleWidget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(GlyphStyleWidget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(GlyphStyleWidget PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(GlyphStyleWidget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
enable_whiskertoolbox_shared_pch(GlyphStyleWidget)