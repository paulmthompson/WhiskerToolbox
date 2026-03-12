find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(MEDIA_EXPORT_SOURCES
    media_export.cpp
    media_export.hpp
    MediaExport_Widget.cpp
    MediaExport_Widget.hpp
    MediaExport_Widget.ui
)

add_library(MediaExport STATIC ${MEDIA_EXPORT_SOURCES})

target_link_libraries(MediaExport PUBLIC Qt6::Widgets Qt6::Core Qt6::Gui)

target_link_libraries(MediaExport PRIVATE CoreUtilities) # just for string manipulation
target_link_libraries(MediaExport PRIVATE MediaData)

target_include_directories(MediaExport PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For DataManager
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_properties(MediaExport PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
)

set_target_compiler_warnings(MediaExport)

enable_whiskertoolbox_shared_pch(MediaExport)