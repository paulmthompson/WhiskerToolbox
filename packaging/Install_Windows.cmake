



# Function to install targets with consistent configuration
function(install_targets target_list)
    foreach(target IN LISTS target_list)
        install(TARGETS ${target}
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
    endforeach()
endfunction()

# Install all main targets
set(MAIN_TARGETS
    DataManager
    DataManagerIO
    ImageProcessing
    TensorData
    MaskData
    LineData 
    MediaData
    TimeFrame
    Entity
    DataViewer
    StateEstimation
    WhiskerToolbox
)

if(ENABLE_OPENCV)
    list(APPEND MAIN_TARGETS DataManagerOpenCV)
endif()
if(ENABLE_CAPNPROTO)
    list(APPEND MAIN_TARGETS DataManagerIO_CapnProto)
endif()
if(ENABLE_HDF5)
    list(APPEND MAIN_TARGETS DataManagerHDF5)
endif()

install_targets("${MAIN_TARGETS}")

# QT thinks thinks that dependency qt6ad
# is a library, and tries to install it, but doesn't find it with qt6 and errors
add_custom_command(TARGET WhiskerToolbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<$<CONFIG:Release>:"${CMAKE_BINARY_DIR}/bin/qt6advanceddocking.dll">
        $<$<CONFIG:Debug>:"${CMAKE_BINARY_DIR}/bin/qt6advanceddockingd.dll">
        "${Qt6_DIR}/../../../bin"
        COMMENT "Copying qt6advanceddocking DLL to Qt directory"
)

#add_custom_command(TARGET WhiskerToolbox POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy_if_different
#        "${CMAKE_BINARY_DIR}/bin/bz2.dll;${CMAKE_BINARY_DIR}/bin/libscip.dll"
#        $<TARGET_FILE_DIR:WhiskerToolbox>
#        COMMAND_EXPAND_LISTS
#)

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
        "${CMAKE_BINARY_DIR}/opencv_photo4.dll"
        "${CMAKE_BINARY_DIR}/opencv_videoio4.dll"
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

set(WHISKER_DLLS
        "${CMAKE_BINARY_DIR}/janelia.dll"
)

set(EXTRA_DLLS
        "${CMAKE_BINARY_DIR}/szip.dll"
        "${CMAKE_BINARY_DIR}/liblzma.dll"
        "${CMAKE_BINARY_DIR}/openblas.dll"
        "${CMAKE_BINARY_DIR}/liblapack.dll"
        "${CMAKE_BINARY_DIR}/libgcc_s_seh-1.dll"
        "${CMAKE_BINARY_DIR}/libgfortran-5.dll"
        "${CMAKE_BINARY_DIR}/libquadmath-0.dll"
        "${CMAKE_BINARY_DIR}/libwinpthread-1.dll"
        #"${CMAKE_BINARY_DIR}/libomp140.x86_64.dll" #This is located in C:\Windows\System32
        "C:/Windows/System32/libomp140.x86_64.dll"
        "${CMAKE_BINARY_DIR}/yyjson.dll"

)

set(TORCH_DLLS
        "${CMAKE_BINARY_DIR}/_deps/torch-src/lib/asmjit.dll"
        "${CMAKE_BINARY_DIR}/_deps/torch-src/lib/c10.dll"
        "${CMAKE_BINARY_DIR}/_deps/torch-src/lib/fbgemm.dll"
        "${CMAKE_BINARY_DIR}/_deps/torch-src/lib/libiomp5md.dll"
        "${CMAKE_BINARY_DIR}/_deps/torch-src/lib/libiompstubs5md.dll"
        "${CMAKE_BINARY_DIR}/_deps/torch-src/lib/torch.dll"
        "${CMAKE_BINARY_DIR}/_deps/torch-src/lib/torch_cpu.dll"
        "${CMAKE_BINARY_DIR}/_deps/torch-src/lib/uv.dll"
)

set(ABSEIL_DLLS
    "${CMAKE_BINARY_DIR}/bin/abseil_dll.dll"
    "${CMAKE_BINARY_DIR}/bin/libprotobuf.dll"
)

copy_dlls_during_install("${OPENCV_DLLS}" "${CMAKE_INSTALL_BINDIR}")
copy_dlls_during_install("${HDF5_DLLS}" "${CMAKE_INSTALL_BINDIR}")
copy_dlls_during_install("${WHISKER_DLLS}" "${CMAKE_INSTALL_BINDIR}")
copy_dlls_during_install("${EXTRA_DLLS}" "${CMAKE_INSTALL_BINDIR}")
copy_dlls_during_install("${TORCH_DLLS}" "${CMAKE_INSTALL_BINDIR}")
copy_dlls_during_install("${ABSEIL_DLLS}" "${CMAKE_INSTALL_BINDIR}")

# ---------------------------------------------------------------------------
# Python standard library — needed by the embedded Python interpreter.
# The PythonEngine sets PYTHONHOME to the exe directory, so the stdlib
# must be in <install>/bin/Lib/ on Windows.
# ---------------------------------------------------------------------------
# Derive the vcpkg Python stdlib location from the found executable.
# On Windows vcpkg the stdlib (Lib/) is typically next to the Python
# executable under tools/python3/ — NOT at the triplet root.
get_filename_component(_py_tools "${Python3_EXECUTABLE}" DIRECTORY)
set(_py_stdlib_src "")

# Try next to the Python executable first
if(EXISTS "${_py_tools}/Lib/encodings/__init__.py")
    set(_py_stdlib_src "${_py_tools}/Lib")
    set(_py_prefix "${_py_tools}")
else()
    # Fallback: triplet root (two levels up from tools/python3/)
    get_filename_component(_py_prefix "${_py_tools}" DIRECTORY)
    get_filename_component(_py_prefix "${_py_prefix}" DIRECTORY)
    set(_py_stdlib_src "${_py_prefix}/Lib")
endif()

if(IS_DIRECTORY "${_py_stdlib_src}")
    install(DIRECTORY "${_py_stdlib_src}/"
            DESTINATION "${CMAKE_INSTALL_BINDIR}/Lib"
            PATTERN "__pycache__" EXCLUDE
            PATTERN "test" EXCLUDE
            PATTERN "tests" EXCLUDE
            PATTERN "tkinter" EXCLUDE
            PATTERN "turtledemo" EXCLUDE
            PATTERN "idlelib" EXCLUDE
            PATTERN "ensurepip" EXCLUDE
            PATTERN "distutils" EXCLUDE
            PATTERN "lib2to3" EXCLUDE
    )
    message(STATUS "[Install] Python stdlib from: ${_py_stdlib_src}")
else()
    message(WARNING "[Install] Python stdlib not found at: ${_py_stdlib_src}")
endif()

# Also install the python3XX.dll if it links dynamically.
# On Windows vcpkg it may be next to the exe (tools/python3/) or in bin/.
set(_py_dll_candidates
    "${_py_prefix}/python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.dll"
    "${_py_prefix}/bin/python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.dll"
)
foreach(_dll_candidate IN LISTS _py_dll_candidates)
    if(EXISTS "${_dll_candidate}")
        install(FILES "${_dll_candidate}" DESTINATION "${CMAKE_INSTALL_BINDIR}")
        message(STATUS "[Install] Python DLL: ${_dll_candidate}")
        break()
    endif()
endforeach()

# Install DLLs directory (C extension modules like _json, _struct, etc.)
set(_py_dlls_src "${_py_prefix}/DLLs")
if(IS_DIRECTORY "${_py_dlls_src}")
    install(DIRECTORY "${_py_dlls_src}/"
            DESTINATION "${CMAKE_INSTALL_BINDIR}/DLLs"
            PATTERN "__pycache__" EXCLUDE
    )
    message(STATUS "[Install] Python DLLs from: ${_py_dlls_src}")
endif()

# Print support and xml are dependencies of JKQTPlotter6
# But they are not automatically found by qt windows deployment
# and need to be added manually
qt_generate_deploy_app_script(
        TARGET WhiskerToolbox
        OUTPUT_SCRIPT deploy_script
        DEPLOY_TOOL_OPTIONS --ignore-library-errors -xml -printsupport
)

install(SCRIPT "${deploy_script}")

