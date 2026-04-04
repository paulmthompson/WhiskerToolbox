# PlottingSVG Library
#
# Qt-free SVG rendering layer for CorePlotting scenes.
# Converts RenderableScene into SVG document strings (publication / export).

find_package(glm CONFIG REQUIRED)

set(PLOTTINGSVG_SOURCES
    Decorations/SVGAxisRenderer.cpp
    Decorations/SVGAxisRenderer.hpp
    Decorations/SVGDecoration.hpp
    Decorations/SVGScalebar.cpp
    Decorations/SVGScalebar.hpp
    Renderers/SVGGlyphRenderer.cpp
    Renderers/SVGGlyphRenderer.hpp
    Renderers/SVGPolyLineRenderer.cpp
    Renderers/SVGPolyLineRenderer.hpp
    Renderers/SVGRectangleRenderer.cpp
    Renderers/SVGRectangleRenderer.hpp
    SVGDocument.cpp
    SVGDocument.hpp
    SVGSceneRenderer.cpp
    SVGSceneRenderer.hpp
    SVGUtils.cpp
    SVGUtils.hpp
)

add_library(PlottingSVG STATIC ${PLOTTINGSVG_SOURCES})

target_include_directories(PlottingSVG PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(PlottingSVG PUBLIC CorePlotting)
target_link_libraries(PlottingSVG PUBLIC glm::glm)

set_target_compiler_warnings(PlottingSVG)

set_target_properties(PlottingSVG PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
enable_whiskertoolbox_pch(PlottingSVG)
