add_library(PlotDataExport INTERFACE)

target_include_directories(PlotDataExport
INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(PlotDataExport INTERFACE GatherResult CorePlotting TimeFrame)
