add_subdirectory(Observer)

add_subdirectory(TimeFrame)

add_subdirectory(Entity)

add_subdirectory(CoreGeometry)

add_subdirectory(SpatialIndex)

add_subdirectory(FeatureExtraction)

if (ENABLE_OPENCV)
    add_subdirectory(ImageProcessing)
endif()
add_subdirectory(StateEstimation)
add_subdirectory(DataManager)
add_subdirectory(CorePlotting)
if (ENABLE_UI)
    add_subdirectory(PlottingOpenGL)
    add_subdirectory(WhiskerToolbox)
endif()
