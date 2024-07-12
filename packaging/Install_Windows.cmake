



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
qt_generate_deploy_app_script(
        TARGET WhiskerToolbox
        OUTPUT_SCRIPT deploy_script
        DEPLOY_TOOL_OPTIONS --ignore-library-errors --no-advanceddocking
)

install(SCRIPT "${deploy_script}")

