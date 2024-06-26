
set_target_properties(WhiskerToolbox PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
)

#include(\"${QT_DEPLOY_SUPPORT}\")
# App bundles on macOS have an .app suffix
if(APPLE)

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

    install(TARGETS DataManager WhiskerToolbox
            BUNDLE DESTINATION .
    )
else()

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

    IF (WIN32)
        install(RUNTIME_DEPENDENCY_SET appDeps
                PRE_EXCLUDE_REGEXES
                [[api-ms-win-.*]] [[ext-ms-.*]] [[kernel32\.dll]]
                [[bcrypt.dll]] [[mfplat.dll]] [[msvcrt.dll]] [[ole32.dll]] [[secur32.dll]] [[user32.dll]] [[vcruntime140.dll]]
                [[ws2_32.dll]]
                [[libgcc_s_seh-1\.dll]] [[libstdc\+\+\-6.dll]]
                POST_EXCLUDE_REGEXES
                [[.*/system32/.*\.dll]]
                [[avcodec*]]
        )
    ELSE()
        install(RUNTIME_DEPENDENCY_SET appDeps
                PRE_EXCLUDE_REGEXES
                [[libc\.so\..*]] [[libgcc_s\.so\..*]] [[libm\.so\..*]] [[libstdc\+\+\.so\..*]]
                [[ld.*]] [[libbz2.*]] [[libdl.*]] [[libgmp.*]] [[libgnutls.*]] [[libhogweed.*]]
                [[libpthread.*]] [[librt.*]] [[libz.*]] [[libQt6Widgets.*]]
                POST_EXCLUDE_REGEXES
                [[/lib/x86_64-linux-gnu/*]]
        )
    ENDIF()

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
endif()

install(SCRIPT "${deploy_script}")

include(CPack)
