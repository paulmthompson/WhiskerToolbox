find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(PLOT_ALIGNMENT_WIDGET_SOURCES

        Core/PlotAlignmentData.hpp
        Core/PlotAlignmentState.cpp
        Core/PlotAlignmentState.hpp

        UI/PlotAlignmentWidget.cpp
        UI/PlotAlignmentWidget.hpp
        UI/PlotAlignmentWidget.ui

)

add_library(PlotAlignmentWidget STATIC ${PLOT_ALIGNMENT_WIDGET_SOURCES})

set_target_properties(PlotAlignmentWidget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(PlotAlignmentWidget PUBLIC 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
)

target_link_libraries(PlotAlignmentWidget PUBLIC DataTypeEnum)
target_link_libraries(PlotAlignmentWidget PRIVATE DataManager GatherResult)

target_include_directories(PlotAlignmentWidget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_compiler_warnings(PlotAlignmentWidget)

# Enable automoc for Qt
set_target_properties(PlotAlignmentWidget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
enable_whiskertoolbox_shared_pch(PlotAlignmentWidget)