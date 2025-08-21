
add_subdirectory(CoreGeometry)
if (ENABLE_OPENCV)
    add_subdirectory(ImageProcessing)
endif()
add_subdirectory(DataManager)
if (ENABLE_UI)
    add_subdirectory(WhiskerToolbox)
endif()
