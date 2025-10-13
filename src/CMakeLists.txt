add_subdirectory(TimeFrame)

add_subdirectory(Entity)

add_subdirectory(CoreGeometry)

add_subdirectory(SpatialIndex)

if (ENABLE_OPENCV)
    add_subdirectory(ImageProcessing)
endif()
add_subdirectory(StateEstimation)
add_subdirectory(DataManager)
if (ENABLE_UI)
    add_subdirectory(WhiskerToolbox)
endif()
