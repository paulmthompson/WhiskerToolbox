Include(FetchContent)

FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1 # or a later release
)

FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

include(Catch)

add_subdirectory(TimeFrame)
add_subdirectory(Entity)
add_subdirectory(CoreGeometry)
add_subdirectory(SpatialIndex)
add_subdirectory(StateEstimation)
add_subdirectory(DataManager)
if (ENABLE_UI)
        add_subdirectory(WhiskerToolbox)
endif()
