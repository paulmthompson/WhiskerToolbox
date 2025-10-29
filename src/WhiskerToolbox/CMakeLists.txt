find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)

message(STATUS ${CMAKE_PREFIX_PATH})

set(CMAKE_CUDA_FLAGS "-allow-unsupported-compiler")
find_package(Torch REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

set(QT_ENABLE_VERBOSE_DEPLOYMENT TRUE)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Xml Svg PrintSupport OpenGLWidgets OpenGL Concurrent)

find_package(LAPACK REQUIRED)
find_package(OpenBLAS REQUIRED)
find_package(Armadillo REQUIRED)

find_package(glm CONFIG REQUIRED)

qt_standard_project_setup()

add_subdirectory(Collapsible_Widget)
add_subdirectory(ColorPicker_Widget)
add_subdirectory(GroupManagementWidget)
add_subdirectory(MediaExport)
add_subdirectory(DataViewer)
add_subdirectory(ShaderManager)
add_subdirectory(TimeScrollBar)
add_subdirectory(Analysis_Dashboard)
add_subdirectory(Feature_Table_Widget)
add_subdirectory(Feature_Tree_Widget)
add_subdirectory(IO_Widgets)
add_subdirectory(DataViewer_Widget)
add_subdirectory(Media_Widget)
add_subdirectory(MediaWidgetManager)
add_subdirectory(TableViewerWidget)
add_subdirectory(TableDesignerWidget)
add_subdirectory(ML_Widget)

set(PROJECT_SOURCES
        main.cpp
        whiskertoolbox.qrc
        my_stylesheet.qss
        color_scheme.hpp

        BatchProcessing_Widget/BatchProcessing_Widget.hpp
        BatchProcessing_Widget/BatchProcessing_Widget.cpp
        BatchProcessing_Widget/BatchProcessing_Widget.ui

        Main_Window/mainwindow.cpp
        Main_Window/mainwindow.hpp
        Main_Window/mainwindow.ui
        Main_Window/video_loader.hpp
        Main_Window/video_loader.cpp

        DataManager_Widget/DataManager_Widget.hpp
        DataManager_Widget/DataManager_Widget.cpp
        DataManager_Widget/DataManager_Widget.ui

        DataManager_Widget/Mask_Widget/Mask_Widget.hpp
        DataManager_Widget/Mask_Widget/Mask_Widget.cpp
        DataManager_Widget/Mask_Widget/Mask_Widget.ui
        DataManager_Widget/Mask_Widget/MaskTableModel.cpp
        DataManager_Widget/Mask_Widget/MaskTableModel.hpp

        DataManager_Widget/Point_Widget/Point_Widget.hpp
        DataManager_Widget/Point_Widget/Point_Widget.cpp
        DataManager_Widget/Point_Widget/Point_Widget.ui
        DataManager_Widget/Point_Widget/PointTableModel.cpp
        DataManager_Widget/Point_Widget/PointTableModel.hpp

        DataManager_Widget/Line_Widget/Line_Widget.hpp
        DataManager_Widget/Line_Widget/Line_Widget.cpp
        DataManager_Widget/Line_Widget/Line_Widget.ui
        DataManager_Widget/Line_Widget/LineTableModel.cpp
        DataManager_Widget/Line_Widget/LineTableModel.hpp

        DataManager_Widget/Image_Widget/Image_Widget.hpp
        DataManager_Widget/Image_Widget/Image_Widget.cpp
        DataManager_Widget/Image_Widget/Image_Widget.ui
        DataManager_Widget/Image_Widget/ImageTableModel.hpp
        DataManager_Widget/Image_Widget/ImageTableModel.cpp

        DataManager_Widget/AnalogTimeSeries_Widget/AnalogTimeSeries_Widget.hpp
        DataManager_Widget/AnalogTimeSeries_Widget/AnalogTimeSeries_Widget.cpp
        DataManager_Widget/AnalogTimeSeries_Widget/AnalogTimeSeries_Widget.ui

        DataManager_Widget/DigitalIntervalSeries_Widget/DigitalIntervalSeries_Widget.hpp
        DataManager_Widget/DigitalIntervalSeries_Widget/DigitalIntervalSeries_Widget.cpp
        DataManager_Widget/DigitalIntervalSeries_Widget/DigitalIntervalSeries_Widget.ui
        DataManager_Widget/DigitalIntervalSeries_Widget/IntervalTableModel.hpp

        DataManager_Widget/DigitalEventSeries_Widget/DigitalEventSeries_Widget.hpp
        DataManager_Widget/DigitalEventSeries_Widget/DigitalEventSeries_Widget.cpp
        DataManager_Widget/DigitalEventSeries_Widget/DigitalEventSeries_Widget.ui
        DataManager_Widget/DigitalEventSeries_Widget/EventTableModel.hpp

        DataManager_Widget/Tensor_Widget/Tensor_Widget.hpp
        DataManager_Widget/Tensor_Widget/Tensor_Widget.cpp
        DataManager_Widget/Tensor_Widget/Tensor_Widget.ui
        DataManager_Widget/Tensor_Widget/TensorTableModel.hpp

        DataManager_Widget/OutputDirectoryWidget/OutputDirectoryWidget.hpp
        DataManager_Widget/OutputDirectoryWidget/OutputDirectoryWidget.ui
        DataManager_Widget/OutputDirectoryWidget/OutputDirectoryWidget.cpp

        DataManager_Widget/NewDataWidget/NewDataWidget.hpp
        DataManager_Widget/NewDataWidget/NewDataWidget.cpp
        DataManager_Widget/NewDataWidget/NewDataWidget.ui

        DataTransform_Widget/DataTransform_Widget.ui
        DataTransform_Widget/DataTransform_Widget.cpp
        DataTransform_Widget/DataTransform_Widget.hpp
        DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp
        DataTransform_Widget/TransformParameter_Widget/DataManagerParameter_Widget.hpp
        DataTransform_Widget/TransformParameter_Widget/DataManagerParameter_Widget.cpp

        DataTransform_Widget/Masks/MaskArea_Widget/MaskArea_Widget.hpp
        DataTransform_Widget/Masks/MaskArea_Widget/MaskArea_Widget.cpp
        DataTransform_Widget/Masks/MaskArea_Widget/MaskArea_Widget.ui
        DataTransform_Widget/Masks/MaskCentroid_Widget/MaskCentroid_Widget.hpp
        DataTransform_Widget/Masks/MaskCentroid_Widget/MaskCentroid_Widget.cpp
        DataTransform_Widget/Masks/MaskCentroid_Widget/MaskCentroid_Widget.ui
        DataTransform_Widget/Masks/MaskConnectedComponent_Widget/MaskConnectedComponent_Widget.hpp
        DataTransform_Widget/Masks/MaskConnectedComponent_Widget/MaskConnectedComponent_Widget.cpp
        DataTransform_Widget/Masks/MaskConnectedComponent_Widget/MaskConnectedComponent_Widget.ui
        DataTransform_Widget/Masks/MaskHoleFilling_Widget/MaskHoleFilling_Widget.hpp
        DataTransform_Widget/Masks/MaskHoleFilling_Widget/MaskHoleFilling_Widget.cpp
        DataTransform_Widget/Masks/MaskHoleFilling_Widget/MaskHoleFilling_Widget.ui
        DataTransform_Widget/Masks/MaskMedianFilter_Widget/MaskMedianFilter_Widget.hpp
        DataTransform_Widget/Masks/MaskMedianFilter_Widget/MaskMedianFilter_Widget.cpp
        DataTransform_Widget/Masks/MaskMedianFilter_Widget/MaskMedianFilter_Widget.ui
        DataTransform_Widget/Masks/MaskPrincipalAxis_Widget/MaskPrincipalAxis_Widget.hpp
        DataTransform_Widget/Masks/MaskPrincipalAxis_Widget/MaskPrincipalAxis_Widget.cpp
        DataTransform_Widget/Masks/MaskPrincipalAxis_Widget/MaskPrincipalAxis_Widget.ui
        DataTransform_Widget/Masks/MaskToLine_Widget/MaskToLine_Widget.cpp
        DataTransform_Widget/Masks/MaskToLine_Widget/MaskToLine_Widget.hpp
        DataTransform_Widget/Masks/MaskToLine_Widget/MaskToLine_Widget.ui
        DataTransform_Widget/Masks/MaskSkeletonize_Widget/MaskSkeletonize_Widget.cpp
        DataTransform_Widget/Masks/MaskSkeletonize_Widget/MaskSkeletonize_Widget.hpp
        DataTransform_Widget/Masks/MaskSkeletonize_Widget/MaskSkeletonize_Widget.ui
        DataTransform_Widget/AnalogTimeSeries/AnalogEventThreshold_Widget/AnalogEventThreshold_Widget.ui
        DataTransform_Widget/AnalogTimeSeries/AnalogEventThreshold_Widget/AnalogEventThreshold_Widget.hpp
        DataTransform_Widget/AnalogTimeSeries/AnalogEventThreshold_Widget/AnalogEventThreshold_Widget.cpp
        DataTransform_Widget/AnalogTimeSeries/AnalogFilter_Widget/AnalogFilter_Widget.hpp
        DataTransform_Widget/AnalogTimeSeries/AnalogFilter_Widget/AnalogFilter_Widget.cpp
        DataTransform_Widget/AnalogTimeSeries/AnalogFilter_Widget/AnalogFilter_Widget.ui
        DataTransform_Widget/AnalogTimeSeries/AnalogIntervalPeak_Widget/AnalogIntervalPeak_Widget.hpp
        DataTransform_Widget/AnalogTimeSeries/AnalogIntervalPeak_Widget/AnalogIntervalPeak_Widget.cpp
        DataTransform_Widget/AnalogTimeSeries/AnalogIntervalPeak_Widget/AnalogIntervalPeak_Widget.ui
        DataTransform_Widget/AnalogTimeSeries/AnalogIntervalThreshold_Widget/AnalogIntervalThreshold_Widget.hpp
        DataTransform_Widget/AnalogTimeSeries/AnalogIntervalThreshold_Widget/AnalogIntervalThreshold_Widget.cpp
        DataTransform_Widget/AnalogTimeSeries/AnalogIntervalThreshold_Widget/AnalogIntervalThreshold_Widget.ui
        DataTransform_Widget/AnalogTimeSeries/AnalogHilbertPhase_Widget/AnalogHilbertPhase_Widget.hpp
        DataTransform_Widget/AnalogTimeSeries/AnalogHilbertPhase_Widget/AnalogHilbertPhase_Widget.cpp
        DataTransform_Widget/AnalogTimeSeries/AnalogHilbertPhase_Widget/AnalogHilbertPhase_Widget.ui
        DataTransform_Widget/AnalogTimeSeries/AnalogScaling_Widget/AnalogScaling_Widget.cpp
        DataTransform_Widget/AnalogTimeSeries/AnalogScaling_Widget/AnalogScaling_Widget.hpp
        DataTransform_Widget/AnalogTimeSeries/AnalogScaling_Widget/AnalogScaling_Widget.ui
        DataTransform_Widget/Lines/LineAngle_Widget/LineAngle_Widget.hpp
        DataTransform_Widget/Lines/LineAngle_Widget/LineAngle_Widget.cpp
        DataTransform_Widget/Lines/LineAngle_Widget/LineAngle_Widget.ui
        DataTransform_Widget/Lines/LineMinDist_Widget/LineMinDist_Widget.hpp
        DataTransform_Widget/Lines/LineMinDist_Widget/LineMinDist_Widget.cpp
        DataTransform_Widget/Lines/LineMinDist_Widget/LineMinDist_Widget.ui
        DataTransform_Widget/Lines/LineAlignment_Widget/LineAlignment_Widget.hpp
        DataTransform_Widget/Lines/LineAlignment_Widget/LineAlignment_Widget.cpp
        DataTransform_Widget/Lines/LineAlignment_Widget/LineAlignment_Widget.ui
        DataTransform_Widget/Lines/LineBaseFlip_Widget/LineBaseFlip_Widget.hpp
        DataTransform_Widget/Lines/LineBaseFlip_Widget/LineBaseFlip_Widget.cpp
        DataTransform_Widget/Lines/LineBaseFlip_Widget/LineBaseFlip_Widget.ui
        DataTransform_Widget/Lines/LineResample_Widget/LineResample_Widget.cpp
        DataTransform_Widget/Lines/LineResample_Widget/LineResample_Widget.hpp
        DataTransform_Widget/Lines/LineResample_Widget/LineResample_Widget.ui
        DataTransform_Widget/Lines/LineCurvature_Widget/LineCurvature_Widget.cpp
        DataTransform_Widget/Lines/LineCurvature_Widget/LineCurvature_Widget.hpp
        DataTransform_Widget/Lines/LineCurvature_Widget/LineCurvature_Widget.ui
        DataTransform_Widget/Lines/LineSubsegment_Widget/LineSubsegment_Widget.cpp
        DataTransform_Widget/Lines/LineSubsegment_Widget/LineSubsegment_Widget.hpp
        DataTransform_Widget/Lines/LineSubsegment_Widget/LineSubsegment_Widget.ui
        DataTransform_Widget/Lines/LinePointExtraction_Widget/LinePointExtraction_Widget.cpp
        DataTransform_Widget/Lines/LinePointExtraction_Widget/LinePointExtraction_Widget.hpp
        DataTransform_Widget/Lines/LinePointExtraction_Widget/LinePointExtraction_Widget.ui
        DataTransform_Widget/Lines/LineClip_Widget/LineClip_Widget.cpp
        DataTransform_Widget/Lines/LineClip_Widget/LineClip_Widget.hpp
        DataTransform_Widget/Lines/LineClip_Widget/LineClip_Widget.ui
        DataTransform_Widget/Lines/Line_Proximity_Grouping/LineProximityGrouping_Widget.cpp
        DataTransform_Widget/Lines/Line_Proximity_Grouping/LineProximityGrouping_Widget.hpp
        DataTransform_Widget/Lines/Line_Proximity_Grouping/LineProximityGrouping_Widget.ui
        DataTransform_Widget/Lines/Line_Kalman_Grouping/LineKalmanGrouping_Widget.cpp
        DataTransform_Widget/Lines/Line_Kalman_Grouping/LineKalmanGrouping_Widget.hpp
        DataTransform_Widget/Lines/Line_Kalman_Grouping/LineKalmanGrouping_Widget.ui
        DataTransform_Widget/Lines/Line_Outlier_Detection/LineOutlierDetection_Widget.cpp
        DataTransform_Widget/Lines/Line_Outlier_Detection/LineOutlierDetection_Widget.hpp
        DataTransform_Widget/Lines/Line_Outlier_Detection/LineOutlierDetection_Widget.ui
        DataTransform_Widget/Lines/LineIndexGrouping_Widget/LineIndexGrouping_Widget.cpp
        DataTransform_Widget/Lines/LineIndexGrouping_Widget/LineIndexGrouping_Widget.hpp
        DataTransform_Widget/Lines/LineIndexGrouping_Widget/LineIndexGrouping_Widget.ui
        DataTransform_Widget/Lines/LineGroupToIntervals_Widget/LineGroupToIntervals_Widget.cpp
        DataTransform_Widget/Lines/LineGroupToIntervals_Widget/LineGroupToIntervals_Widget.hpp
        DataTransform_Widget/Lines/LineGroupToIntervals_Widget/LineGroupToIntervals_Widget.ui
        DataTransform_Widget/Points/PointParticleFilter_Widget/PointParticleFilter_Widget.cpp
        DataTransform_Widget/Points/PointParticleFilter_Widget/PointParticleFilter_Widget.hpp
        DataTransform_Widget/Points/PointParticleFilter_Widget/PointParticleFilter_Widget.ui
        DataTransform_Widget/DigitalIntervalSeries/GroupIntervals_Widget/GroupIntervals_Widget.cpp
        DataTransform_Widget/DigitalIntervalSeries/GroupIntervals_Widget/GroupIntervals_Widget.hpp
        DataTransform_Widget/DigitalIntervalSeries/GroupIntervals_Widget/GroupIntervals_Widget.ui
        DataTransform_Widget/DigitalIntervalSeries/BooleanOperation_Widget/BooleanOperation_Widget.cpp
        DataTransform_Widget/DigitalIntervalSeries/BooleanOperation_Widget/BooleanOperation_Widget.hpp
        DataTransform_Widget/DigitalIntervalSeries/BooleanOperation_Widget/BooleanOperation_Widget.ui
        DataTransform_Widget/Media/WhiskerTracing_Widget/WhiskerTracing_Widget.hpp
        DataTransform_Widget/Media/WhiskerTracing_Widget/WhiskerTracing_Widget.cpp
        DataTransform_Widget/Media/WhiskerTracing_Widget/WhiskerTracing_Widget.ui

        Whisker_Widget/Whisker_Widget.hpp
        Whisker_Widget/Whisker_Widget.ui
        Whisker_Widget/Whisker_Widget.cpp
        Whisker_Widget/janelia_config.ui
        Whisker_Widget/janelia_config.hpp
        Whisker_Widget/janelia_config.cpp

        Tongue_Widget/Tongue_Widget.hpp
        Tongue_Widget/Tongue_Widget.cpp
        Tongue_Widget/Tongue_Widget.ui

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

        utils/qt_utilities.hpp
        utils/qt_utilities.cpp

        utils/Deep_Learning/Backbones/efficientvit.hpp
        utils/Deep_Learning/Backbones/efficientvit.cpp

        utils/Deep_Learning/torch_helpers.hpp

        utils/Deep_Learning/scm.hpp
        utils/Deep_Learning/scm.cpp

        utils/Deep_Learning/Models/EfficientSAM/EfficientSAM.hpp
        utils/Deep_Learning/Models/EfficientSAM/EfficientSAM.cpp

        Terminal_Widget/TerminalWidget.hpp
        Terminal_Widget/TerminalWidget.cpp
        Terminal_Widget/TerminalWidget.ui

        Test_Widget/Test_Widget.hpp
        Test_Widget/Test_Widget.cpp
        Test_Widget/Test_Widget.ui
)

qt_add_executable(WhiskerToolbox
        ${PROJECT_SOURCES}
)

target_link_libraries(WhiskerToolbox PRIVATE Qt6::Widgets Qt6::PrintSupport Qt6::Xml Qt6::OpenGLWidgets Qt6::OpenGL Qt6::Concurrent)

target_link_libraries(WhiskerToolbox PRIVATE nlohmann_json::nlohmann_json)

target_link_libraries(WhiskerToolbox PRIVATE glm::glm)

message(STATUS ${TORCH_LIBRARIES})
target_link_libraries(WhiskerToolbox PRIVATE "${TORCH_LIBRARIES}")

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
target_link_libraries(WhiskerToolbox PRIVATE Collapsible_Widget)
target_link_libraries(WhiskerToolbox PRIVATE ColorPicker_Widget)
target_link_libraries(WhiskerToolbox PRIVATE IO_Widget)
target_link_libraries(WhiskerToolbox PRIVATE DataViewer_Widget)
target_link_libraries(WhiskerToolbox PRIVATE Media_Widget)
target_link_libraries(WhiskerToolbox PRIVATE MediaWidgetManager)
target_link_libraries(WhiskerToolbox PRIVATE TableViewerWidget)
target_link_libraries(WhiskerToolbox PRIVATE TableDesignerWidget)

# Conditionally link CapnProto plugin for Binary IO widgets
if(ENABLE_CAPNPROTO)
    target_link_libraries(WhiskerToolbox PRIVATE DataManagerIO_CapnProto)
endif()

# Conditionally link HDF5 plugin
if(ENABLE_HDF5)
    target_link_libraries(WhiskerToolbox PRIVATE DataManagerHDF5)
endif()

target_link_libraries(WhiskerToolbox PRIVATE SpatialIndex)
target_link_libraries(WhiskerToolbox PUBLIC NEURALYZER_GEOMETRY)
target_link_libraries(WhiskerToolbox PRIVATE ShaderManager)
target_link_libraries(WhiskerToolbox PRIVATE TimeScrollBar)
target_link_libraries(WhiskerToolbox PRIVATE Analysis_Dashboard)
target_link_libraries(WhiskerToolbox PRIVATE Feature_Table_Widget)
target_link_libraries(WhiskerToolbox PRIVATE Feature_Tree_Widget)
target_link_libraries(WhiskerToolbox PRIVATE ML_Widget)


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
