



install(TARGETS DataManager
        BUNDLE DESTINATION .
        LIBRARY
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)


install(TARGETS WhiskerToolbox
        BUNDLE DESTINATION .
        LIBRARY
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# QT thinks thinks that dependency qt6ad
# is a library, and tries to install it, but doesn't find it with qt6 and errors
add_custom_command(TARGET WhiskerToolbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<$<CONFIG:Release>:"${CMAKE_BINARY_DIR}/bin/qt6advanceddocking.dll">
        $<$<CONFIG:Debug>:"${CMAKE_BINARY_DIR}/bin/qt6advanceddockingd.dll">
        "${Qt6_DIR}/../../../bin"
        COMMENT "Copying qt6advanceddocking DLL to Qt directory"
)

function(copy_dlls_during_install dll_list dest_dir)
    foreach(dll IN LISTS dll_list)
        install(CODE "
            execute_process(
                COMMAND \${CMAKE_COMMAND} -E copy_if_different
                \"${dll}\"
                \"\${CMAKE_INSTALL_PREFIX}/${dest_dir}\"
            )
            message(STATUS \"Copying ${dll} to install directory\")
        ")
    endforeach()
endfunction()

set(OPENCV_DLLS
        "${CMAKE_BINARY_DIR}/opencv_imgproc4.dll"
        "${CMAKE_BINARY_DIR}/opencv_core4.dll"
        "${CMAKE_BINARY_DIR}/opencv_imgcodecs4.dll"
        "${CMAKE_BINARY_DIR}/jpeg62.dll"
        "${CMAKE_BINARY_DIR}/libwebpdecoder.dll"
        "${CMAKE_BINARY_DIR}/libwebp.dll"
        "${CMAKE_BINARY_DIR}/tiff.dll"
        "${CMAKE_BINARY_DIR}/libpng16.dll"
        "${CMAKE_BINARY_DIR}/libsharpyuv.dll"
        "${CMAKE_BINARY_DIR}/zlib1.dll"
)

set(HDF5_DLLS
        "${CMAKE_BINARY_DIR}/hdf5_cpp.dll"
        "${CMAKE_BINARY_DIR}/hdf5.dll"
)

set(JKQTPLOTTER_DLLS
        $<$<CONFIG:Release>:"${CMAKE_BINARY_DIR}/JKQTPlotter6_Release.dll">
        $<$<CONFIG:Release>:"${CMAKE_BINARY_DIR}/JKQTMath6_Release.dll">
        $<$<CONFIG:Release>:"${CMAKE_BINARY_DIR}/JKQTMathText6_Release.dll">
        $<$<CONFIG:Release>:"${CMAKE_BINARY_DIR}/JKQTCommon6_Release.dll">
)

set(WHISKER_DLLS
        "${CMAKE_BINARY_DIR}/janelia.dll"
)

set(EXTRA_DLLS
        "${CMAKE_BINARY_DIR}/szip.dll"
        "${CMAKE_BINARY_DIR}/liblzma.dll"
)

copy_dlls_during_install("${OPENCV_DLLS}" "${CMAKE_INSTALL_BINDIR}")
copy_dlls_during_install("${HDF5_DLLS}" "${CMAKE_INSTALL_BINDIR}")
copy_dlls_during_install("${WHISKER_DLLS}" "${CMAKE_INSTALL_BINDIR}")
copy_dlls_during_install("${EXTRA_DLLS}" "${CMAKE_INSTALL_BINDIR}")
copy_dlls_during_install("${JKQTPLOTTER_DLLS}" "${CMAKE_INSTALL_BINDIR}")

qt_generate_deploy_app_script(
        TARGET WhiskerToolbox
        OUTPUT_SCRIPT deploy_script
        DEPLOY_TOOL_OPTIONS --ignore-library-errors
)

install(SCRIPT "${deploy_script}")

