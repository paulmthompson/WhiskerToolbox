find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui OpenGL OpenGLWidgets)

set(SPECTROGRAM_WIDGET_SOURCES

        Core/SpectrogramState.cpp
        Core/SpectrogramState.hpp

        SpectrogramWidgetRegistration.cpp
        SpectrogramWidgetRegistration.hpp

        UI/SpectrogramWidget.cpp
        UI/SpectrogramWidget.hpp
        UI/SpectrogramWidget.ui
        UI/SpectrogramPropertiesWidget.cpp
        UI/SpectrogramPropertiesWidget.hpp
        UI/SpectrogramPropertiesWidget.ui

)

add_library(SpectrogramWidget STATIC ${SPECTROGRAM_WIDGET_SOURCES})

set_target_properties(SpectrogramWidget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(SpectrogramWidget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

target_link_libraries(SpectrogramWidget PRIVATE DataManager)
target_link_libraries(SpectrogramWidget PRIVATE EditorState)

target_include_directories(SpectrogramWidget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(SpectrogramWidget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(SpectrogramWidget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(SpectrogramWidget PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(SpectrogramWidget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
