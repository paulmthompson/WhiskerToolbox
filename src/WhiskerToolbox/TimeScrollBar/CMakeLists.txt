# TimeScrollBar Library CMakeLists.txt

# Find required packages
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

# Define the TimeScrollBar library sources
set(TIMESCROLLBAR_SOURCES
    TimeScrollBar.hpp
    TimeScrollBar.cpp
    TimeScrollBar.ui
)

# Add conditional video support
if(ENABLE_FFMPEG)
    list(APPEND TIMESCROLLBAR_SOURCES TimeScrollBar_Video.cpp)
else()
    list(APPEND TIMESCROLLBAR_SOURCES TimeScrollBar_NoVideo.cpp)
endif()

# Create the TimeScrollBar library
add_library(TimeScrollBar STATIC ${TIMESCROLLBAR_SOURCES})

# Set target properties
set_target_properties(TimeScrollBar PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Link Qt libraries
target_link_libraries(TimeScrollBar PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
)

target_link_libraries(TimeScrollBar PRIVATE DataManager)

# Conditionally link to MediaData when FFMPEG is enabled for video-specific functionality
if(ENABLE_FFMPEG)
    target_link_libraries(TimeScrollBar PRIVATE MediaData)
    #target_compile_definitions(TimeScrollBar PRIVATE ENABLE_FFMPEG)
endif()

# Include directories
target_include_directories(TimeScrollBar PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../DataManager>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compile definitions
target_compile_definitions(TimeScrollBar PRIVATE _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(TimeScrollBar PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(TimeScrollBar PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(TimeScrollBar PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(TimeScrollBar PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
) 