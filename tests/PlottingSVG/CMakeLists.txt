set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

find_package(glm CONFIG REQUIRED)

add_executable(test_PlottingSVG
    SVGUtils.test.cpp
    SVGDocument.test.cpp
    SVGPolyLineRenderer.test.cpp
    SVGGlyphRenderer.test.cpp
    SVGRectangleRenderer.test.cpp
    SVGSceneRenderer.test.cpp
    SVGScalebar.test.cpp
    SVGAxisRenderer.test.cpp
    EventPlotExportIntegration.test.cpp
)

target_link_libraries(test_PlottingSVG PRIVATE
    Catch2::Catch2WithMain
    PlottingSVG
    Entity
    glm::glm
)

target_include_directories(test_PlottingSVG PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
)

if (ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_PlottingSVG)
else()
    add_test(NAME test_PlottingSVG_all
        COMMAND test_PlottingSVG
    )
endif()
