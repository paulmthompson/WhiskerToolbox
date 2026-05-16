add_executable(DataManagerPipelineRunner
        main.cpp
)

target_link_libraries(DataManagerPipelineRunner PRIVATE DataManager)
target_link_libraries(DataManagerPipelineRunner PRIVATE Commands)

set_target_compiler_warnings(DataManagerPipelineRunner)
