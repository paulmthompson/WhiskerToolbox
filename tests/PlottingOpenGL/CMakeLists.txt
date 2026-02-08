# PlottingOpenGL Integration Tests
#
# Headless GL 4.3 tests for the LineBatch components:
#   - GLSSBOBuffer      — SSBO RAII wrapper
#   - BatchLineStore    — GPU buffer management + CPU mirror
#   - ComputeShaderIntersector — GPU/CPU intersection equivalence
#   - BatchLineRenderer — Initialization and render smoke tests
#
# These tests create a headless OpenGL 4.3 Core context via QOffscreenSurface.
# They gracefully SKIP on systems without GL 4.3 (macOS, software renderers).

# Currently only supported on Linux (CI uses headless display via Xvfb)
if (APPLE)
    message(STATUS "PlottingOpenGL tests currently not supported on macOS")
    return()
endif()

if (WIN32)
    message(STATUS "PlottingOpenGL tests currently not supported on Windows")
    return()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui OpenGL OpenGLWidgets Widgets)
find_package(glm CONFIG REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

add_executable(test_plottingopengl
    GLSSBOBuffer.test.cpp
    BatchLineStore.test.cpp
    ComputeShaderIntersector.test.cpp
    BatchLineRenderer.test.cpp
)

# Enable Qt MOC (needed for ShaderManager which contains Q_OBJECT)
set_target_properties(test_plottingopengl PROPERTIES AUTOMOC TRUE)

target_link_libraries(test_plottingopengl PRIVATE
    Catch2::Catch2WithMain
    PlottingOpenGL
    CorePlotting
    glm::glm
    Qt6::Core
    Qt6::Gui
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Widgets
)

target_include_directories(test_plottingopengl PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
)

# The compute shader lives in the WhiskerToolbox shaders directory.
# Pass the filesystem path so tests can load it without needing the qrc.
target_compile_definitions(test_plottingopengl PRIVATE
    TEST_SHADER_DIR="${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/shaders"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_plottingopengl PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_plottingopengl PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(test_plottingopengl PRIVATE ${MSVC_WARNINGS})
endif()

if (ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_plottingopengl
        PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_plottingopengl_all
        COMMAND test_plottingopengl
    )
    set_property(TEST test_plottingopengl_all
        PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
endif()
