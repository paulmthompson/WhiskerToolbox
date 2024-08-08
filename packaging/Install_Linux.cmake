

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

install(TARGETS DataManager RUNTIME_DEPENDENCY_SET appDeps)

install(RUNTIME_DEPENDENCY_SET appDeps
                PRE_EXCLUDE_REGEXES
                [[libc\.so\..*]] [[libgcc_s\.so\..*]] [[libm\.so\..*]] [[libstdc\+\+\.so\..*]]
                [[ld.*]] [[libbz2.*]] [[libdl.*]] [[libgmp.*]] [[libgnutls.*]] [[libhogweed.*]]
                [[libpthread.*]] [[librt.*]] [[libz.*]] [[libQt6Widgets.*]]
                POST_EXCLUDE_REGEXES
                [[/lib/x86_64-linux-gnu/*]]
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

qt_generate_deploy_app_script(
        TARGET WhiskerToolbox
        OUTPUT_SCRIPT deploy_script
)

install(SCRIPT "${deploy_script}")


