add_executable(DataManagerPipelineRunner
        main.cpp
)

target_link_libraries(DataManagerPipelineRunner PRIVATE DataManager)
target_link_libraries(DataManagerPipelineRunner PRIVATE Commands)
target_link_libraries(DataManagerPipelineRunner PRIVATE DeepLearning)
target_link_libraries(DataManagerPipelineRunner PRIVATE WhiskerToolbox::PythonRuntime)

# MediaData, DataSynthesizer, and TransformsV2 use file-scope registration objects.
# Force whole-archive linking so headless JSON workflows see the same registrations
# as the GUI and command tests.
if(APPLE)
    # Link CMake targets for transitive static-lib deps (e.g. TransformsV2 -> MLCore).
    # -force_load alone only injects the .a path and skips that propagation.
    target_link_libraries(DataManagerPipelineRunner PRIVATE
            MediaData DataSynthesizer TransformsV2
            -Wl,-force_load,$<TARGET_FILE:MediaData>
            -Wl,-force_load,$<TARGET_FILE:DataSynthesizer>
            -Wl,-force_load,$<TARGET_FILE:TransformsV2>
    )
elseif(MSVC)
    target_link_options(DataManagerPipelineRunner PRIVATE
            /WHOLEARCHIVE:MediaData
            /WHOLEARCHIVE:DataSynthesizer
            /WHOLEARCHIVE:TransformsV2
    )
    target_link_libraries(DataManagerPipelineRunner PRIVATE
            MediaData DataSynthesizer TransformsV2)
else()
    target_link_libraries(DataManagerPipelineRunner PRIVATE
            -Wl,--whole-archive MediaData DataSynthesizer TransformsV2 -Wl,--no-whole-archive
    )
endif()

if(ENABLE_SPIKE2_SONPY)
    target_link_libraries(DataManagerPipelineRunner PRIVATE DataManagerIO_Spike2SonPy)
    target_compile_definitions(DataManagerPipelineRunner PRIVATE ENABLE_SPIKE2_SONPY)
endif()

set_target_compiler_warnings(DataManagerPipelineRunner)
