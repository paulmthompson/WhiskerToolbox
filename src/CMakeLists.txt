add_subdirectory(Observer)

add_subdirectory(TimeFrame)

add_subdirectory(Entity)

add_subdirectory(CoreGeometry)

add_subdirectory(CoreUtilities)

add_subdirectory(CoreMath)

add_subdirectory(SpatialIndex)

add_subdirectory(FeatureExtraction)

if (ENABLE_OPENCV)
    add_subdirectory(ImageProcessing)
endif()

add_subdirectory(DataObjects)

add_subdirectory(IO)

add_subdirectory(StateEstimation)

add_subdirectory(DataManager)
add_subdirectory(ParameterSchema)
add_subdirectory(DataSynthesizer)
add_subdirectory(TriageSession)
add_subdirectory(TransformsV2)
add_subdirectory(GatherResult)

add_subdirectory(MLCore)

add_subdirectory(python_bindings)
add_subdirectory(CorePlotting)
add_subdirectory(DeepLearning)

add_subdirectory(Commands)
if (ENABLE_UI)
    add_subdirectory(PlottingOpenGL)
    add_subdirectory(WhiskerToolbox)
endif()
