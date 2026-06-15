add_executable(DataManagerPipelineRunner
        main.cpp
)

target_link_libraries(DataManagerPipelineRunner PRIVATE DataManager)
target_link_libraries(DataManagerPipelineRunner PRIVATE Commands)
target_link_libraries(DataManagerPipelineRunner PRIVATE TransformsV2)
target_link_libraries(DataManagerPipelineRunner PRIVATE DeepLearning)

# MediaData and DataSynthesizer both use file-scope registration objects.
# Force whole-archive linking so headless JSON workflows see the same registrations
# as the GUI and command tests.
if(APPLE)
    target_link_libraries(DataManagerPipelineRunner PRIVATE
            -Wl,-force_load,$<TARGET_FILE:MediaData>
            -Wl,-force_load,$<TARGET_FILE:DataSynthesizer>
    )
elseif(MSVC)
    target_link_options(DataManagerPipelineRunner PRIVATE
            /WHOLEARCHIVE:MediaData
            /WHOLEARCHIVE:DataSynthesizer
    )
    target_link_libraries(DataManagerPipelineRunner PRIVATE MediaData DataSynthesizer)
else()
    target_link_libraries(DataManagerPipelineRunner PRIVATE
            -Wl,--whole-archive MediaData DataSynthesizer -Wl,--no-whole-archive
    )
endif()

set_target_compiler_warnings(DataManagerPipelineRunner)
