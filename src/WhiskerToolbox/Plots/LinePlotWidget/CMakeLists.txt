find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(LINE_PLOT_WIDGET_SOURCES

        Core/LinePlotState.cpp
        Core/LinePlotState.hpp

        LinePlotWidgetRegistration.cpp
        LinePlotWidgetRegistration.hpp

        UI/LinePlotWidget.cpp
        UI/LinePlotWidget.hpp
        UI/LinePlotWidget.ui
        UI/LinePlotPropertiesWidget.cpp
        UI/LinePlotPropertiesWidget.hpp
        UI/LinePlotPropertiesWidget.ui

)

add_library(LinePlotWidget STATIC ${LINE_PLOT_WIDGET_SOURCES})

set_target_properties(LinePlotWidget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(LinePlotWidget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
)

target_link_libraries(LinePlotWidget PRIVATE DataManager)
target_link_libraries(LinePlotWidget PRIVATE EditorState)
target_link_libraries(LinePlotWidget PRIVATE PlotAlignmentWidget)

target_include_directories(LinePlotWidget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(LinePlotWidget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(LinePlotWidget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(LinePlotWidget PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(LinePlotWidget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
