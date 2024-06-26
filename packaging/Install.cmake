
set_target_properties(WhiskerToolbox PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
)

if (APPLE)
    include(${CMAKE_SOURCE_DIR}/packaging/Install_Apple.cmake)
elseif (WIN32)
    include(${CMAKE_SOURCE_DIR}/packaging/Install_Windows.cmake)
else()
    include(${CMAKE_SOURCE_DIR}/packaging/Install_Linux.cmake)
endif()

include(CPack)
