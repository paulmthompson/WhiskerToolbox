

# ---------------------------------------------------------------------------
# Collect all project shared libraries so RUNTIME_DEPENDENCY_SET resolves
# their transitive .so dependencies (vcpkg OpenCV, HDF5, torch, etc.).
# ---------------------------------------------------------------------------
set(LINUX_SHARED_TARGETS
        DataManagerIO
        DataManagerNumpy
        StateEstimation
        Whisker-Analysis
        janelia
        qt6advanceddocking
)

if(ENABLE_OPENCV)
    list(APPEND LINUX_SHARED_TARGETS DataManagerOpenCV)
endif()
if(ENABLE_HDF5)
    list(APPEND LINUX_SHARED_TARGETS DataManagerHDF5)
endif()
if(ENABLE_CAPNPROTO)
    list(APPEND LINUX_SHARED_TARGETS DataManagerIO_CapnProto)
endif()
if(ENABLE_FFMPEG)
    list(APPEND LINUX_SHARED_TARGETS ffmpeg_wrapper)
endif()

# Install project shared libraries and add them to the dependency set.
install(TARGETS ${LINUX_SHARED_TARGETS}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
foreach(_tgt IN LISTS LINUX_SHARED_TARGETS)
    install(TARGETS ${_tgt} RUNTIME_DEPENDENCY_SET appDeps)
endforeach()

set_target_properties(WhiskerToolbox PROPERTIES
        INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
)

install(TARGETS WhiskerToolbox
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        RUNTIME_DEPENDENCY_SET appDeps
)

install(RUNTIME_DEPENDENCY_SET appDeps
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        DIRECTORIES
                "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib"
                "${Qt6_DIR}/../.."
        PRE_EXCLUDE_REGEXES
                [[libc\.so\..*]] [[libgcc_s\.so\..*]] [[libm\.so\..*]] [[libstdc\+\+\.so\..*]]
                [[ld.*]] [[libbz2.*]] [[libdl.*]] [[libgmp.*]] [[libgnutls.*]] [[libhogweed.*]]
                [[libpthread.*]] [[librt.*]] [[libz.*]]
        POST_EXCLUDE_REGEXES
                [[/lib/x86_64-linux-gnu/*]]
)

# ---------------------------------------------------------------------------
# CMake's RUNTIME_DEPENDENCY_SET does not add the executable to the
# EXECUTABLES list of file(GET_RUNTIME_DEPENDENCIES).  Resolve the
# executable's own direct shared-library dependencies explicitly so
# that libraries inherited from static archives (e.g. DataManager)
# are also installed.
# ---------------------------------------------------------------------------
set(_wt_exe "$<TARGET_FILE:WhiskerToolbox>")
set(_search_dirs
        "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib"
        "${Qt6_DIR}/../.."
        "${CMAKE_BINARY_DIR}/_deps/torch-src/lib"
)

install(CODE "
    file(GET_RUNTIME_DEPENDENCIES
        EXECUTABLES \"${_wt_exe}\"
        RESOLVED_DEPENDENCIES_VAR _exe_deps
        UNRESOLVED_DEPENDENCIES_VAR _exe_unresolved
        DIRECTORIES ${_search_dirs}
        PRE_EXCLUDE_REGEXES
            [[libc\\.so\\..*]] [[libgcc_s\\.so\\..*]] [[libm\\.so\\..*]] [[libstdc\\+\\+\\.so\\..*]]
            [[ld.*]] [[libbz2.*]] [[libdl.*]] [[libgmp.*]] [[libgnutls.*]] [[libhogweed.*]]
            [[libpthread.*]] [[librt.*]] [[libz.*]]
        POST_EXCLUDE_REGEXES
            [[/lib/x86_64-linux-gnu/*]]
    )
    foreach(_dep IN LISTS _exe_deps)
        # Skip libraries already installed by the dependency set above
        get_filename_component(_depname \"\${_dep}\" NAME)
        if(NOT EXISTS \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/\${_depname}\")
            file(INSTALL DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}\"
                 TYPE SHARED_LIBRARY FILES \"\${_dep}\" FOLLOW_SYMLINK_CHAIN)
        endif()
    endforeach()
")

qt_generate_deploy_app_script(
        TARGET WhiskerToolbox
        OUTPUT_SCRIPT deploy_script
)

install(SCRIPT "${deploy_script}")


