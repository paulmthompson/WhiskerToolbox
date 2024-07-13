


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
            execute_process(
                COMMAND cp -f \"${dylib}\" \"\${CMAKE_INSTALL_PREFIX}/${dest_dir}\"
            )
            message(STATUS \"Copying ${dylib} to WhiskerToolbox.app/Contents/Frameworks\")
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

copy_dylibs_during_install("${MY_DYLIBS}" "WhiskerToolbox.app/Contents/Frameworks")

install(TARGETS DataManager WhiskerToolbox
        BUNDLE DESTINATION .
)

install(SCRIPT "${deploy_script}")

