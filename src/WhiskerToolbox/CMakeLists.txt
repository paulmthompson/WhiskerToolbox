find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)

message(STATUS ${CMAKE_PREFIX_PATH})


set(QT_ENABLE_VERBOSE_DEPLOYMENT TRUE)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Xml Svg PrintSupport OpenGLWidgets OpenGL Concurrent)

find_package(LAPACK REQUIRED)
find_package(OpenBLAS REQUIRED)
find_package(Armadillo REQUIRED)

find_package(glm CONFIG REQUIRED)

qt_standard_project_setup()

add_subdirectory(EditorState) # reflectcpp, Qt
add_subdirectory(KeymapSystem) # Qt, EditorState
add_subdirectory(Collapsible_Widget) # Qt
add_subdirectory(Terminal_Widget) # Qt, EditorState, reflectcpp
add_subdirectory(ColorPicker_Widget) # Qt, DataManager

add_subdirectory(GroupManagementWidget) # Qt, DataManager, EditorState
add_subdirectory(GroupContextMenu) # Qt, Entity, GroupManagementWidget

add_subdirectory(MediaExport) # Qt, DataManager
add_subdirectory(DataViewer) # DataManager, glm

 # Qt, DataManager, EditorState, MediaData (optional), reflectcpp
add_subdirectory(TimeScrollBar)

add_subdirectory(Feature_Table_Widget) # Qt, DataManager
add_subdirectory(Feature_Tree_Widget) # Qt, DataManager
add_subdirectory(TimeFrame_Table_Widget) # Qt, DataManager

add_subdirectory(TableViewerWidget) # Qt, DataManager

add_subdirectory(ZoneManager) #Qt, EditorState, reflectcpp, qt6advanceddocking

add_subdirectory(StateManagement) # Qt, reflectcpp, nlohmann_json, EditorState, ZoneManager

add_subdirectory(Scaling_Widget) # Qt, CoreGeometry - shared widget for image scaling

add_subdirectory(DataExport_Widget) # Qt, DataManager, Scaling_Widget

# Qt, DataManager, EditorState, Scaling_Widget - data import UI
add_subdirectory(DataImport_Widget)

# File format explorer widgets (NumPy, etc.)
add_subdirectory(FileExplorer_Widgets)

# Qt, DataManager, DataManagerIO, TensorData,
# EditorState, MediaExport, Feature_Table_Widget,
# IO_Widget, Collapsible_Widget, Entity,
# GroupManagementWidget, nlohmann_json
add_subdirectory(DataManager_Widget)

# Qt, DataManager, EditorState, GroupManagementWidget, nlohmann_json
# Data inspection widget with View/Properties split
add_subdirectory(DataInspector_Widget)

# Qt, DataManager, glm, Boost,
# DataViewer, Feature_Tree_Widget, PlottingOpenGL,
# ColorPicker_Widget, TimeScrollbar, CorePlotting
add_subdirectory(DataViewer_Widget)

# Qt, DataManager, Feature_Table_Widget, EditorState,
# Geometry, Collapsible_Widget, ColorPicker_Widget,
# GroupManagementWidget, GroupContextMenu, opencv,
# TimeScrollBar
add_subdirectory(Media_Widget)

# Qt, DataManager, TableViewerWidget, Collapsible_Widget
add_subdirectory(TableDesignerWidget)

# Qt, DataManager, TensorData,
# OpenBLAS, armadillo, LAPACK, nlohmann_json, MLPACK
# Feature_Table_Widget, EditorState
add_subdirectory(ML_Widget)

# Qt, DataManager, EditorState, reflectcpp - MLCore ML workflow UI
add_subdirectory(MLCore_Widget)

# Qt, DataManager, DeepLearning, EditorState
add_subdirectory(DeepLearning_Widget)

 # Qt, DataManager, EditorState, Geometry
add_subdirectory(DataTransform_Widget)

# Qt, ParameterSchema - Generic parameter form widget
add_subdirectory(AutoParamWidget)

# Qt, DataManager, EditorState - TransformsV2 pipeline UI
add_subdirectory(TransformsV2_Widget)

# Qt, DataManager, EditorState, TriageSession - Triage session UI
add_subdirectory(TriageSession_Widget)

# Qt, DataManager - Shared plot alignment widget
add_subdirectory(Plots/Common/PlotAlignmentWidget)

# Qt, CorePlotting - Shared relative time axis widget
add_subdirectory(Plots/Common/RelativeTimeAxisWidget)
add_subdirectory(Plots/Common/VerticalAxisWidget)
add_subdirectory(Plots/Common/HorizontalAxisWidget)
add_subdirectory(Plots/Common/GlyphStyleWidget)
add_subdirectory(Plots/Common/LineStyleControls)

# Qt - Shared tooltip manager for plot widgets
add_subdirectory(Plots/Common/TooltipManager)

# DataManager, GatherResult, PlotAlignmentWidget - shared firing-rate estimation (no Qt)
add_subdirectory(Plots/Common/EventRateEstimation)

# Qt, EventRateEstimation - Composable UI controls for rate estimation method & scaling
add_subdirectory(Plots/Common/RateEstimationControls)

# Qt, CorePlotting - Composable colormap selection and color range controls
add_subdirectory(Plots/Common/ColormapControls)

# Qt, EditorState, reflectcpp - Python integration widget
add_subdirectory(Python_Widget)

# Qt, DataManager, EditorState - Event raster plot widget
add_subdirectory(Plots/EventPlotWidget)

# Qt, DataManager, EditorState - Autocorrelation function plot widget
add_subdirectory(Plots/ACFWidget)

# Qt, DataManager, EditorState - PSTH plot widget
add_subdirectory(Plots/PSTHWidget)

# Qt, DataManager, EditorState - Line plot widget
add_subdirectory(Plots/LinePlotWidget)

# Qt, DataManager, EditorState - Heatmap plot widget
add_subdirectory(Plots/HeatmapWidget)

# Qt, DataManager, EditorState - Temporal projection view widget
add_subdirectory(Plots/TemporalProjectionViewWidget)

# Qt, DataManager, EditorState - Onion skin view widget
add_subdirectory(Plots/OnionSkinViewWidget)

# Qt, DataManager, EditorState - Scatter plot widget
add_subdirectory(Plots/ScatterPlotWidget)

# Qt, DataManager, EditorState - Spectrogram widget
add_subdirectory(Plots/SpectrogramWidget)

# Qt, DataManager, EditorState - 3D plot widget
add_subdirectory(Plots/3DPlot)

# Qt - Layout sanity checking for development and testing
add_subdirectory(LayoutTesting)

# Qt, DataManager, EditorState, reflectcpp - Test/proof-of-concept widget
add_subdirectory(Test_Widget)

# Qt, DataManager, EditorState, reflectcpp - Data synthesizer widget
add_subdirectory(DataSynthesizer_Widget)

# Qt, EditorState, Commands, reflectcpp - Command log widget
add_subdirectory(CommandLog_Widget)

# Qt, KeymapSystem, EditorState - Configurable keybinding editor widget
add_subdirectory(KeybindingEditor)

set(PROJECT_SOURCES
        main.cpp
        whiskertoolbox.qrc
        my_stylesheet.qss
        color_scheme.hpp

        BatchProcessing_Widget/BatchProcessing_Widget.hpp
        BatchProcessing_Widget/BatchProcessing_Widget.cpp
        BatchProcessing_Widget/BatchProcessing_Widget.ui
        BatchProcessing_Widget/BatchProcessingState.hpp
        BatchProcessing_Widget/BatchProcessingState.cpp
        BatchProcessing_Widget/BatchProcessingWidgetRegistration.hpp
        BatchProcessing_Widget/BatchProcessingWidgetRegistration.cpp

        Main_Window/mainwindow.cpp
        Main_Window/mainwindow.hpp
        Main_Window/mainwindow.ui
        Main_Window/video_loader.hpp
        Main_Window/video_loader.cpp

        Whisker_Widget/Whisker_Widget.hpp
        Whisker_Widget/Whisker_Widget.ui
        Whisker_Widget/Whisker_Widget.cpp
        Whisker_Widget/WhiskerWidgetState.hpp
        Whisker_Widget/WhiskerWidgetState.cpp
        Whisker_Widget/WhiskerWidgetRegistration.hpp
        Whisker_Widget/WhiskerWidgetRegistration.cpp
        Whisker_Widget/janelia_config.ui
        Whisker_Widget/janelia_config.hpp
        Whisker_Widget/janelia_config.cpp

        Tongue_Widget/Tongue_Widget.hpp
        Tongue_Widget/Tongue_Widget.cpp
        Tongue_Widget/Tongue_Widget.ui
        Tongue_Widget/TongueWidgetState.hpp
        Tongue_Widget/TongueWidgetState.cpp
        Tongue_Widget/TongueWidgetRegistration.hpp
        Tongue_Widget/TongueWidgetRegistration.cpp

        Grabcut_Widget/Grabcut_Widget.hpp
        Grabcut_Widget/Grabcut_Widget.cpp
        Grabcut_Widget/GrabCutTool.cpp
        Grabcut_Widget/GrabCutTool.hpp
        Grabcut_Widget/Grabcut_Widget.ui

        Magic_Eraser_Widget/magic_eraser.hpp
        Magic_Eraser_Widget/magic_eraser.cpp

        Export_Widgets/Export_Video_Widget/Export_Video_Widget.hpp
        Export_Widgets/Export_Video_Widget/Export_Video_Widget.cpp
        Export_Widgets/Export_Video_Widget/Export_Video_Widget.ui
        Export_Widgets/Export_Video_Widget/ExportVideoWidgetState.hpp
        Export_Widgets/Export_Video_Widget/ExportVideoWidgetState.cpp
        Export_Widgets/Export_Video_Widget/ExportVideoWidgetRegistration.hpp
        Export_Widgets/Export_Video_Widget/ExportVideoWidgetRegistration.cpp

        utils/qt_utilities.hpp
        utils/qt_utilities.cpp

        utils/DataLoadUtils.hpp
        utils/DataLoadUtils.cpp

)

qt_add_executable(WhiskerToolbox
        ${PROJECT_SOURCES}
)

target_link_libraries(WhiskerToolbox PRIVATE Qt6::Widgets Qt6::PrintSupport Qt6::Xml Qt6::OpenGLWidgets Qt6::OpenGL Qt6::Concurrent)

target_link_libraries(WhiskerToolbox PRIVATE nlohmann_json::nlohmann_json)

target_link_libraries(WhiskerToolbox PRIVATE glm::glm)

target_link_libraries(WhiskerToolbox PRIVATE spdlog::spdlog_header_only)


target_include_directories(WhiskerToolbox PRIVATE

  #      "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/AnalogViewer>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/TableDesignerWidget>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Label_Widget>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Main_Window>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Whisker_Widget>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Grabcut_Widget>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../MediaExport>"

        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(WhiskerToolbox PRIVATE Whisker-Analysis)
target_link_libraries(WhiskerToolbox PRIVATE DataManager DataManagerIO TensorData)
target_link_libraries(WhiskerToolbox PRIVATE ImageProcessing)
target_link_libraries(WhiskerToolbox PRIVATE MediaExport)
target_link_libraries(WhiskerToolbox PRIVATE EditorState)
target_link_libraries(WhiskerToolbox PRIVATE KeymapSystem)
target_link_libraries(WhiskerToolbox PRIVATE Collapsible_Widget)
target_link_libraries(WhiskerToolbox PRIVATE ColorPicker_Widget)
target_link_libraries(WhiskerToolbox PRIVATE DataExport_Widget)
target_link_libraries(WhiskerToolbox PRIVATE DataImport_Widget)
target_link_libraries(WhiskerToolbox PRIVATE DataViewer_Widget)
target_link_libraries(WhiskerToolbox PRIVATE Media_Widget)
target_link_libraries(WhiskerToolbox PRIVATE TableViewerWidget)
target_link_libraries(WhiskerToolbox PRIVATE TableDesignerWidget)
target_link_libraries(WhiskerToolbox PRIVATE DataManager_Widget)
target_link_libraries(WhiskerToolbox PRIVATE DataInspector_Widget)
target_link_libraries(WhiskerToolbox PRIVATE Terminal_Widget)
target_link_libraries(WhiskerToolbox PRIVATE Python_Widget)
target_link_libraries(WhiskerToolbox PRIVATE ZoneManager)
target_link_libraries(WhiskerToolbox PRIVATE StateManagement)

# Force whole-archive linking for MediaData to ensure static media type/loader registration
# (VideoDataRegistration.cpp, ImageDataRegistration.cpp etc. have file-scope RAII objects
# that register into MediaDataFactory's singleton; without --whole-archive the linker strips them)
if(APPLE)
    target_link_libraries(WhiskerToolbox PRIVATE -Wl,-force_load,$<TARGET_FILE:MediaData>)
elseif(MSVC)
    target_link_options(WhiskerToolbox PRIVATE /WHOLEARCHIVE:MediaData)
    target_link_libraries(WhiskerToolbox PRIVATE MediaData)
else()
    target_link_libraries(WhiskerToolbox PRIVATE -Wl,--whole-archive MediaData -Wl,--no-whole-archive)
endif()

# Force whole-archive linking for DeepLearning to ensure static model registration runs
# (NeuroSAMModel self-registers via DL_REGISTER_MODEL macro, but linker would strip it
# since there are no direct symbol references)
if(APPLE)
    target_link_libraries(WhiskerToolbox PRIVATE -Wl,-force_load,$<TARGET_FILE:DeepLearning>)
elseif(MSVC)
    # MSVC requires /WHOLEARCHIVE as a linker option, not a library name
    target_link_options(WhiskerToolbox PRIVATE /WHOLEARCHIVE:DeepLearning)
    target_link_libraries(WhiskerToolbox PRIVATE DeepLearning)
    #target_link_libraries(WhiskerToolbox PRIVATE /WHOLEARCHIVE:DeepLearning)
else()
    # Linux/GCC/Clang
    target_link_libraries(WhiskerToolbox PRIVATE -Wl,--whole-archive DeepLearning -Wl,--no-whole-archive)
endif()

target_link_libraries(WhiskerToolbox PRIVATE DeepLearning_Widget)

# Conditionally link CapnProto plugin for Binary IO widgets
if(ENABLE_CAPNPROTO)
    target_link_libraries(WhiskerToolbox PRIVATE DataManagerIO_CapnProto)
endif()

# Conditionally link HDF5 plugin
if(ENABLE_HDF5)
    target_link_libraries(WhiskerToolbox PRIVATE DataManagerHDF5)
    target_link_libraries(WhiskerToolbox PRIVATE HDF5Explorer_Widget)
    target_compile_definitions(WhiskerToolbox PRIVATE ENABLE_HDF5)
endif()

# File format explorer widgets
target_link_libraries(WhiskerToolbox PRIVATE NpyExplorer_Widget)
target_link_libraries(WhiskerToolbox PRIVATE BinaryExplorer_Widget)
target_link_libraries(WhiskerToolbox PRIVATE CsvExplorer_Widget)

target_link_libraries(WhiskerToolbox PRIVATE SpatialIndex)
target_link_libraries(WhiskerToolbox PUBLIC NEURALYZER_GEOMETRY)
target_link_libraries(WhiskerToolbox PRIVATE PlottingOpenGL)  # Contains ShaderManager
target_link_libraries(WhiskerToolbox PRIVATE TimeScrollBar)
target_link_libraries(WhiskerToolbox PRIVATE Feature_Table_Widget)
target_link_libraries(WhiskerToolbox PRIVATE Feature_Tree_Widget)
target_link_libraries(WhiskerToolbox PRIVATE ML_Widget)
target_link_libraries(WhiskerToolbox PRIVATE MLCore_Widget)
target_link_libraries(WhiskerToolbox PRIVATE DataTransform_Widget)
target_link_libraries(WhiskerToolbox PRIVATE TransformsV2_Widget)
target_link_libraries(WhiskerToolbox PRIVATE TriageSession_Widget)

# Force whole-archive linking for TransformsV2 to ensure static transform registration
# (RegisteredTransforms.cpp has file-scope RAII objects that register transforms into
# ElementRegistry's singleton; without --whole-archive the linker strips them)
if(APPLE)
    target_link_libraries(WhiskerToolbox PRIVATE -Wl,-force_load,$<TARGET_FILE:TransformsV2>)
elseif(MSVC)
    target_link_options(WhiskerToolbox PRIVATE /WHOLEARCHIVE:TransformsV2)
    target_link_libraries(WhiskerToolbox PRIVATE TransformsV2)
else()
    target_link_libraries(WhiskerToolbox PRIVATE -Wl,--whole-archive TransformsV2 -Wl,--no-whole-archive)
endif()

# Force whole-archive linking for DataSynthesizer to ensure static generator registration
# (Generator .cpp files have file-scope RegisterGenerator<Params> RAII objects that register
# into GeneratorRegistry's singleton; without --whole-archive the linker strips them)
if(APPLE)
    target_link_libraries(WhiskerToolbox PRIVATE -Wl,-force_load,$<TARGET_FILE:DataSynthesizer>)
elseif(MSVC)
    target_link_options(WhiskerToolbox PRIVATE /WHOLEARCHIVE:DataSynthesizer)
    target_link_libraries(WhiskerToolbox PRIVATE DataSynthesizer)
else()
    target_link_libraries(WhiskerToolbox PRIVATE -Wl,--whole-archive DataSynthesizer -Wl,--no-whole-archive)
endif()

target_link_libraries(WhiskerToolbox PRIVATE PlotAlignmentWidget)
target_link_libraries(WhiskerToolbox PRIVATE EventPlotWidget)
target_link_libraries(WhiskerToolbox PRIVATE ACFWidget)
target_link_libraries(WhiskerToolbox PRIVATE PSTHWidget)
target_link_libraries(WhiskerToolbox PRIVATE LinePlotWidget)
target_link_libraries(WhiskerToolbox PRIVATE HeatmapWidget)
target_link_libraries(WhiskerToolbox PRIVATE TemporalProjectionViewWidget)
target_link_libraries(WhiskerToolbox PRIVATE ScatterPlotWidget)
target_link_libraries(WhiskerToolbox PRIVATE SpectrogramWidget)
target_link_libraries(WhiskerToolbox PRIVATE 3DPlotWidget)
target_link_libraries(WhiskerToolbox PRIVATE OnionSkinViewWidget)
target_link_libraries(WhiskerToolbox PRIVATE Test_Widget)
target_link_libraries(WhiskerToolbox PRIVATE DataSynthesizer_Widget)
target_link_libraries(WhiskerToolbox PRIVATE CommandLog_Widget)
target_link_libraries(WhiskerToolbox PRIVATE KeybindingEditor)
target_link_libraries(WhiskerToolbox PRIVATE LayoutTesting)

target_link_libraries(WhiskerToolbox PRIVATE opencv_core opencv_imgproc opencv_highgui opencv_photo)

target_link_libraries(WhiskerToolbox PRIVATE armadillo)
target_link_libraries(WhiskerToolbox PRIVATE LAPACK::LAPACK)
target_link_libraries(WhiskerToolbox PRIVATE OpenBLAS::OpenBLAS)

target_link_libraries(WhiskerToolbox PRIVATE qt6advanceddocking)
target_link_libraries(WhiskerToolbox PRIVATE ffmpeg_wrapper::ffmpeg_wrapper)

target_compile_definitions(WhiskerToolbox PRIVATE _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)


if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(WhiskerToolbox PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(WhiskerToolbox PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(WhiskerToolbox PRIVATE ${MSVC_WARNINGS})
endif()

set_target_properties(WhiskerToolbox PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(${CMAKE_SOURCE_DIR}/packaging/Install.cmake)
enable_whiskertoolbox_pch(WhiskerToolbox)