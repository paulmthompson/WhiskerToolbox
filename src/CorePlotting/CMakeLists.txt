
find_package(glm CONFIG REQUIRED)

set(COREPLOTTING_SOURCES
    # Mappers (visualization-specific coordinate mapping)
    Mappers/MappedElement.hpp
    Mappers/MappedLineView.hpp
    Mappers/MapperConcepts.hpp
    Mappers/TimeSeriesMapper.hpp
    Mappers/TimeSeriesMapper.cpp
    Mappers/SpatialMapper.hpp
    Mappers/RasterMapper.hpp
    Mappers/HistogramMapper.hpp
    Mappers/HistogramMapper.cpp
    # DataTypes (style, cache - used by widgets)
    DataTypes/SeriesStyle.hpp
    DataTypes/SeriesDataCache.hpp
    DataTypes/HistogramData.hpp
    # SceneGraph
    SceneGraph/RenderablePrimitives.hpp
    SceneGraph/RenderablePrimitives.cpp
    SceneGraph/SceneBuilder.hpp
    SceneGraph/SceneBuilder.cpp
    # CoordinateTransform
    CoordinateTransform/InverseTransform.cpp 
    CoordinateTransform/InverseTransform.hpp
    CoordinateTransform/AxisMapping.hpp
    CoordinateTransform/AxisMapping.cpp
    CoordinateTransform/ViewState.hpp
    CoordinateTransform/ViewState.cpp
    CoordinateTransform/ViewStateData.hpp
    CoordinateTransform/TimeRange.hpp
    CoordinateTransform/TimeAxisCoordinates.hpp
    CoordinateTransform/TimeAxisCoordinates.cpp
    CoordinateTransform/TimeFrameAdapters.hpp
    CoordinateTransform/SeriesCoordinateQuery.hpp
    CoordinateTransform/SeriesCoordinateQuery.cpp
    CoordinateTransform/SeriesMatrices.hpp
    CoordinateTransform/SeriesMatrices.cpp
    # Layout
    Layout/LayoutTransform.hpp
    Layout/NormalizationHelpers.hpp
    Layout/SeriesLayout.hpp
    Layout/LayoutEngine.hpp
    Layout/LayoutEngine.cpp
    Layout/StackedLayoutStrategy.hpp
    Layout/StackedLayoutStrategy.cpp
    Layout/RowLayoutStrategy.hpp
    Layout/RowLayoutStrategy.cpp
    Layout/SpatialLayoutStrategy.hpp
    Layout/SpatialLayoutStrategy.cpp
    # Transformers
    Transformers/GapDetector.hpp
    Transformers/GapDetector.cpp

    # Interaction
    Interaction/GlyphPreview.hpp
    Interaction/IGlyphInteractionController.hpp
    Interaction/DataCoordinates.hpp
    Interaction/RectangleInteractionController.hpp
    Interaction/RectangleInteractionController.cpp
    Interaction/LineInteractionController.hpp
    Interaction/LineInteractionController.cpp
    Interaction/PolygonInteractionController.hpp
    Interaction/PolygonInteractionController.cpp
    Interaction/HitTestResult.hpp
    Interaction/SceneHitTester.cpp
    Interaction/SceneHitTester.hpp
    # Export
    Export/SVGPrimitives.hpp
    Export/SVGPrimitives.cpp
)

add_library(CorePlotting STATIC ${COREPLOTTING_SOURCES}
    )

target_include_directories(CorePlotting PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>" 
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )

target_link_libraries(CorePlotting PUBLIC NEURALYZER_GEOMETRY)
target_link_libraries(CorePlotting PUBLIC WhiskerToolbox::ObserverData)
target_link_libraries(CorePlotting PUBLIC WhiskerToolbox::TimeFrame)
target_link_libraries(CorePlotting PUBLIC WhiskerToolbox::Entity)
target_link_libraries(CorePlotting PUBLIC DataManager)
target_link_libraries(CorePlotting PUBLIC SpatialIndex)

target_link_libraries(CorePlotting PUBLIC glm::glm)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(CorePlotting PRIVATE ${CLANG_OPTIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(CorePlotting PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(CorePlotting PRIVATE ${MSVC_WARNINGS})
endif()

set_target_properties(CorePlotting PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
