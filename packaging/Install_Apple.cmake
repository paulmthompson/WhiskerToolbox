


set(executable_path "$<TARGET_FILE_NAME:WhiskerToolbox>.app")
set(data_manager_path "lib/$<TARGET_FILE_NAME:DataManager>")

# Generate a deployment script to be executed at install time
qt_generate_deploy_script(
        TARGET WhiskerToolbox
        OUTPUT_SCRIPT deploy_script
        CONTENT "
   qt_deploy_runtime_dependencies(
   EXECUTABLE ${executable_path}
   ADDITIONAL_LIBRARIES ${data_manager_path}
   GENERATE_QT_CONF
   VERBOSE
   )")

# Need to manually install janelia, whisker analysis, qtdocking and data manager
function(copy_dylibs_during_install dylib_list dest_dir)
    foreach(dylib IN LISTS dylib_list)
        install(CODE "
            set(target_dir \"\${CMAKE_INSTALL_PREFIX}/${dest_dir}\")
            if(NOT EXISTS \"\${target_dir}\")
                file(MAKE_DIRECTORY \"\${target_dir}\")
            endif()
            execute_process(
                COMMAND \${CMAKE_COMMAND} -E copy_if_different \"${dylib}\" \"\${target_dir}\"
            )
            message(STATUS \"Copying ${dylib} to \${target_dir}\")
        ")
    endforeach()
endfunction()

# Example usage
set(MY_DYLIBS
        "${CMAKE_BINARY_DIR}/libjanelia.dylib"
        "${CMAKE_BINARY_DIR}/libWhisker-Analysis.dylib"
        "${CMAKE_BINARY_DIR}/bin/libqt6advanceddocking.dylib"
        "${CMAKE_BINARY_DIR}/libDataManager.dylib"
)

# For each target representing a dynamic library, set the INSTALL_RPATH property
foreach(target IN ITEMS janelia Whisker-Analysis qt6advanceddocking DataManager)
    set_target_properties(${target} PROPERTIES
            INSTALL_RPATH "@executable_path/../Frameworks"
            BUILD_WITH_INSTALL_RPATH TRUE
    )
endforeach()

set_target_properties(WhiskerToolbox PROPERTIES
        INSTALL_RPATH "@executable_path/../Frameworks"
        BUILD_WITH_INSTALL_RPATH TRUE
)

# Define a custom target for updating the install name
add_custom_target(UpdateInstallNameJanelia ALL
        COMMAND ${CMAKE_INSTALL_NAME_TOOL} -id "@executable_path/../Frameworks/libjanelia.dylib" "${CMAKE_BINARY_DIR}/libjanelia.dylib"
        COMMENT "Updating install name for libjanelia.dylib"
)

# Ensure the custom target is built after the janelia target
add_dependencies(UpdateInstallNameJanelia janelia)

copy_dylibs_during_install("${MY_DYLIBS}" "WhiskerToolbox.app/Contents/Frameworks")

install(TARGETS DataManager WhiskerToolbox
        BUNDLE DESTINATION .
)

install(SCRIPT "${deploy_script}")

