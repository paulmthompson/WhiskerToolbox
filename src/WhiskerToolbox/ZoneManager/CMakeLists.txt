
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(reflectcpp CONFIG REQUIRED)


set(ZONE_MANAGER_SOURCES
    ZoneConfig.hpp
    ZoneManager.cpp
    ZoneManager.hpp
    ZoneManagerWidget.hpp
    ZoneManagerWidget.cpp
    ZoneManagerWidgetRegistration.hpp
    ZoneManagerWidgetRegistration.cpp
    EditorCreationController.hpp
    EditorCreationController.cpp
    SplitButtonHandler.hpp
    SplitButtonHandler.cpp
)

add_library(ZoneManager STATIC
    ${ZONE_MANAGER_SOURCES}
)

target_link_libraries(ZoneManager
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        EditorState
)

target_link_libraries(ZoneManager PRIVATE reflectcpp::reflectcpp)
target_link_libraries(ZoneManager PRIVATE qt6advanceddocking)
target_link_libraries(ZoneManager PRIVATE WhiskerToolbox::StateManagement)

target_include_directories(ZoneManager PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_compiler_warnings(ZoneManager)

set_target_properties(ZoneManager PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
enable_whiskertoolbox_shared_pch(ZoneManager)