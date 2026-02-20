
add_library(GatherResult STATIC
        GatherResult.hpp
        GatherPipelineExecutor.hpp
        GatherPipelineExecutor.cpp
)

target_include_directories(GatherResult
PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>" # For TransformsV2 and DataManager
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(GatherResult PUBLIC TransformsV2 DataManager)
