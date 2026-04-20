# TimeScrollBar Library CMakeLists.txt

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

find_package(reflectcpp CONFIG REQUIRED)

# Define the TimeScrollBar library sources
set(TIMESCROLLBAR_SOURCES
    TimeScrollBar.hpp
    TimeScrollBar.cpp
    TimeScrollBarPlayback.hpp
    TimeScrollBar.ui
    TimeScrollBarState.hpp
    TimeScrollBarState.cpp
    TimeScrollBarRegistration.hpp
    TimeScrollBarRegistration.cpp
)

# Add conditional video support
if(ENABLE_FFMPEG)
    list(APPEND TIMESCROLLBAR_SOURCES TimeScrollBar_Video.cpp)
else()
    list(APPEND TIMESCROLLBAR_SOURCES TimeScrollBar_NoVideo.cpp)
endif()

add_library(TimeScrollBar STATIC ${TIMESCROLLBAR_SOURCES})

set_target_properties(TimeScrollBar PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(TimeScrollBar PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
)

target_link_libraries(TimeScrollBar PRIVATE DataManager)
target_link_libraries(TimeScrollBar PRIVATE EditorState)
target_link_libraries(TimeScrollBar PRIVATE KeymapSystem)

# Conditionally link to MediaData when FFMPEG is enabled for video-specific functionality
if(ENABLE_FFMPEG)
    target_link_libraries(TimeScrollBar PRIVATE MediaData)
    #target_compile_definitions(TimeScrollBar PRIVATE ENABLE_FFMPEG)
endif()

target_link_libraries(TimeScrollBar PRIVATE reflectcpp::reflectcpp)

# Include directories
target_include_directories(TimeScrollBar PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_compiler_warnings(TimeScrollBar)

# Enable automoc for Qt
set_target_properties(TimeScrollBar PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
) 
enable_whiskertoolbox_shared_pch(TimeScrollBar)