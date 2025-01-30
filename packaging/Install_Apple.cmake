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
function(copy_items_during_install items_list dest_dir)
    foreach(item IN LISTS items_list)
        install(CODE "
            set(target_dir \"\${CMAKE_INSTALL_PREFIX}/${dest_dir}\")
            if(NOT EXISTS \"\${target_dir}\")
                file(MAKE_DIRECTORY \"\${target_dir}\")
            endif()
            execute_process(
                COMMAND \${CMAKE_COMMAND} -E copy_if_different \"${item}\" \"\${target_dir}\"
            )
            message(STATUS \"Copying ${item} to \${target_dir}\")
        ")
    endforeach()
endfunction()

# Example usage
set(MY_DYLIBS
        "${CMAKE_BINARY_DIR}/libjanelia.dylib"
        "${CMAKE_BINARY_DIR}/libWhisker-Analysis.dylib"
        "${CMAKE_BINARY_DIR}/bin/libqt6advanceddocking.dylib"
        "${CMAKE_BINARY_DIR}/libDataManager.dylib"
        "${CMAKE_BINARY_DIR}/_deps/jkqtplotter6-build/output/libJKQTPlotter6_Release.5.0.0.dylib"
        "${CMAKE_BINARY_DIR}/_deps/jkqtplotter6-build/output/libJKQTMath6_Release.5.0.0.dylib"
        "${CMAKE_BINARY_DIR}/_deps/jkqtplotter6-build/output/libJKQTMathText6_Release.5.0.0.dylib"
        "${CMAKE_BINARY_DIR}/_deps/jkqtplotter6-build/output/libJKQTCommon6_Release.5.0.0.dylib"
        "${CMAKE_BINARY_DIR}/_deps/torch-src/lib/libc10.dylib"
        "${CMAKE_BINARY_DIR}/_deps/torch-src/lib/libomp.dylib"
        "${CMAKE_BINARY_DIR}/_deps/torch-src/lib/libtorch.dylib"
        "${CMAKE_BINARY_DIR}/_deps/torch-src/lib/libtorch_cpu.dylib"
)

set_target_properties(WhiskerToolbox PROPERTIES
        INSTALL_RPATH "@executable_path/../Frameworks"
        BUILD_WITH_INSTALL_RPATH TRUE
)

function(update_install_name target_lib new_install_name old_install_name)
    add_custom_target(UpdateInstallName${target_lib} ALL
            COMMAND ${CMAKE_INSTALL_NAME_TOOL} -id "${new_install_name}" "${CMAKE_BINARY_DIR}/${old_install_name}"
            COMMENT "Updating install name for ${target_lib}"
    )
    add_dependencies(UpdateInstallName${target_lib} ${target_lib})
endfunction()

# Example usage of the function
update_install_name("janelia" "@executable_path/../Frameworks/libjanelia.dylib" "libjanelia.dylib" )
update_install_name("Whisker-Analysis" "@executable_path/../Frameworks/libWhisker-Analysis.dylib" "libWhisker-Analysis.dylib")
update_install_name("DataManager" "@executable_path/../Frameworks/libDataManager.dylib" "libDataManager.dylib")
update_install_name("qt6advanceddocking" "@executable_path/../Frameworks/libqt6advanceddocking.dylib" "bin/libqt6advanceddocking.4.3.1.dylib")
update_install_name("JKQTPlotter6" "@executable_path/../Frameworks/libJKQTPlotter6_Release.5.0.0.dylib" "_deps/jkqtplotter6-build/output/libJKQTPlotter6_Release.5.0.0.dylib")
#update_install_name("Torch" "@executable_path/../Frameworks/libtorch.dylib" "_deps/torch-src/lib/libtorch.dylib")

copy_items_during_install("${MY_DYLIBS}" "WhiskerToolbox.app/Contents/Frameworks")
copy_items_during_install("${CMAKE_SOURCE_DIR}/packaging/WhiskerToolbox.icns" "WhiskerToolbox.app/Contents/Resources/")

install(TARGETS DataManager WhiskerToolbox
        BUNDLE DESTINATION .
)

install(SCRIPT "${deploy_script}")

install(CODE "
    execute_process(
        COMMAND codesign --force --deep --verbose --sign \"Eric Certificate\" \"${CMAKE_INSTALL_PREFIX}/WhiskerToolbox.app\"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT \${result} EQUAL 0)
        message(FATAL_ERROR \"Failed to codesign WhiskerToolbox.app: \${error}\")
    endif()
    message(STATUS \"Successfully codesigned WhiskerToolbox.app\")
    
")