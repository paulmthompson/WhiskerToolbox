
set_target_properties(WhiskerToolbox PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
)

add_custom_command(TARGET WhiskerToolbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:WhiskerToolbox>/resources
    )

# Ship bundled Python helpers (for example Spike2/SonPy) with installed binaries.
if(APPLE)
    set(_WT_RESOURCES_INSTALL_DESTINATION "WhiskerToolbox.app/Contents/MacOS/resources")
else()
    set(_WT_RESOURCES_INSTALL_DESTINATION "resources")
endif()

install(DIRECTORY ${CMAKE_SOURCE_DIR}/resources/
        DESTINATION ${_WT_RESOURCES_INSTALL_DESTINATION}
        USE_SOURCE_PERMISSIONS
        PATTERN "__pycache__" EXCLUDE
        PATTERN "*.pyc" EXCLUDE)

# Example that follows the above guidelines
set(CPACK_PACKAGE_NAME WhiskerToolbox)
set(CPACK_PACKAGE_VENDOR PMT)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Neuro Analysis Software")
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_VERBATIM_VARIABLES YES)
#set(CPACK_PACKAGE_DESCRIPTION_FILE ${CMAKE_CURRENT_LIST_DIR}/Description.txt)
#set(CPACK_RESOURCE_FILE_WELCOME ${CMAKE_CURRENT_LIST_DIR}/Welcome.txt)
#set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_LIST_DIR}/License.txt)
#set(CPACK_RESOURCE_FILE_README ${CMAKE_CURRENT_LIST_DIR}/Readme.txt)

if (APPLE)
    include(${CMAKE_SOURCE_DIR}/packaging/Install_Apple.cmake)
elseif (WIN32)
    include(${CMAKE_SOURCE_DIR}/packaging/Install_Windows.cmake)
else()
    include(${CMAKE_SOURCE_DIR}/packaging/Install_Linux.cmake)
endif()

include(CPack)
