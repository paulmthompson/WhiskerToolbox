
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

target_include_directories(ZoneManager PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)


if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ZoneManager PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ZoneManager PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(ZoneManager PRIVATE ${MSVC_WARNINGS})
endif()

set_target_properties(ZoneManager PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)