# PlottingOpenGL Library
# 
# OpenGL-based rendering layer for CorePlotting scenes.
# Uses Qt6's OpenGL wrappers for cross-platform function loading.
#
# This library consumes RenderableScene from CorePlotting and issues
# OpenGL draw calls. It provides batch renderers for each primitive type:
#   - PolyLineRenderer: For AnalogTimeSeries, segmented lines
#   - GlyphRenderer: For DigitalEventSeries, point markers
#   - RectangleRenderer: For DigitalIntervalSeries, filled regions

find_package(Qt6 REQUIRED COMPONENTS Core OpenGL OpenGLWidgets)
find_package(glm CONFIG REQUIRED)

set(PLOTTINGOPENGL_SOURCES
    GLContext.cpp
    GLContext.hpp
    SceneRenderer.cpp
    SceneRenderer.hpp
    Renderers/IBatchRenderer.hpp
    Renderers/PolyLineRenderer.cpp
    Renderers/PolyLineRenderer.hpp
    Renderers/GlyphRenderer.cpp
    Renderers/GlyphRenderer.hpp
    Renderers/RectangleRenderer.cpp
    Renderers/RectangleRenderer.hpp
)

add_library(PlottingOpenGL STATIC ${PLOTTINGOPENGL_SOURCES})

target_include_directories(PlottingOpenGL PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>" 
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )

# Core dependencies
target_link_libraries(PlottingOpenGL PUBLIC CorePlotting)
target_link_libraries(PlottingOpenGL PUBLIC glm::glm)

# Qt6 OpenGL (for GL function loading and shader compilation)
target_link_libraries(PlottingOpenGL PUBLIC Qt6::Core Qt6::OpenGL Qt6::OpenGLWidgets)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(PlottingOpenGL PRIVATE ${CLANG_OPTIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(PlottingOpenGL PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(PlottingOpenGL PRIVATE ${MSVC_WARNINGS})
endif()

set_target_properties(PlottingOpenGL PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
