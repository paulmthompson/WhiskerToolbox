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
add_subdirectory(python_bindings)
add_subdirectory(CorePlotting)
add_subdirectory(DeepLearning)
if (ENABLE_UI)
    add_subdirectory(PlottingOpenGL)
    add_subdirectory(WhiskerToolbox)
endif()
add_subdirectory(PythonBindings)
