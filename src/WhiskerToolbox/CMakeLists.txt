find_package(HighFive CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)

message(STATUS ${CMAKE_PREFIX_PATH})

set(CMAKE_CUDA_FLAGS "-allow-unsupported-compiler")
find_package(Torch REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

set(QT_ENABLE_VERBOSE_DEPLOYMENT TRUE)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Xml Svg PrintSupport OpenGLWidgets OpenGL)

find_package(LAPACK REQUIRED)
find_package(OpenBLAS REQUIRED)
find_package(Armadillo REQUIRED)

find_package(glm CONFIG REQUIRED)

qt_standard_project_setup()

add_subdirectory(DataManager)
add_subdirectory(DataViewer)

set(PROJECT_SOURCES
        main.cpp
        whiskertoolbox.qrc
        my_stylesheet.qss
        color_scheme.hpp

        ColorPicker_Widget/ColorPicker_Widget.hpp
        ColorPicker_Widget/ColorPicker_Widget.cpp
        ColorPicker_Widget/ColorPicker_Widget.ui

        Main_Window/mainwindow.cpp
        Main_Window/mainwindow.hpp
        Main_Window/mainwindow.ui

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
        DataManager_Widget/Point_Widget/PointTableModel.hpp

        DataManager_Widget/Line_Widget/Line_Widget.hpp
        DataManager_Widget/Line_Widget/Line_Widget.cpp
        DataManager_Widget/Line_Widget/Line_Widget.ui
        DataManager_Widget/Line_Widget/LineTableModel.cpp
        DataManager_Widget/Line_Widget/LineTableModel.hpp

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
        DataTransform_Widget/DigitalIntervalSeries/GroupIntervals_Widget/GroupIntervals_Widget.cpp
        DataTransform_Widget/DigitalIntervalSeries/GroupIntervals_Widget/GroupIntervals_Widget.hpp
        DataTransform_Widget/DigitalIntervalSeries/GroupIntervals_Widget/GroupIntervals_Widget.ui

        Whisker_Widget/Whisker_Widget.hpp
        Whisker_Widget/Whisker_Widget.ui
        Whisker_Widget/Whisker_Widget.cpp
        Whisker_Widget/janelia_config.ui
        Whisker_Widget/janelia_config.hpp
        Whisker_Widget/janelia_config.cpp

        Media_Window/Media_Window.hpp
        Media_Window/Media_Window.cpp

        Media_Widget/Media_Widget.hpp
        Media_Widget/Media_Widget.cpp
        Media_Widget/Media_Widget.ui
        Media_Widget/DisplayOptions/DisplayOptions.hpp
        Media_Widget/DisplayOptions/CoordinateTypes.hpp

        Media_Widget/MediaProcessing_Widget/MediaProcessing_Widget.cpp
        Media_Widget/MediaProcessing_Widget/MediaProcessing_Widget.hpp
        Media_Widget/MediaProcessing_Widget/MediaProcessing_Widget.ui
        Media_Widget/MediaProcessing_Widget/ProcessingOptions/ContrastWidget.cpp
        Media_Widget/MediaProcessing_Widget/ProcessingOptions/ContrastWidget.hpp
        Media_Widget/MediaProcessing_Widget/ProcessingOptions/ContrastWidget.ui
        Media_Widget/MediaProcessing_Widget/ProcessingOptions/GammaWidget.cpp
        Media_Widget/MediaProcessing_Widget/ProcessingOptions/GammaWidget.hpp
        Media_Widget/MediaProcessing_Widget/ProcessingOptions/GammaWidget.ui
        Media_Widget/MediaProcessing_Widget/ProcessingOptions/SharpenWidget.cpp
        Media_Widget/MediaProcessing_Widget/ProcessingOptions/SharpenWidget.hpp
        Media_Widget/MediaProcessing_Widget/ProcessingOptions/SharpenWidget.ui
        Media_Widget/MediaProcessing_Widget/ProcessingOptions/BilateralWidget.cpp
        Media_Widget/MediaProcessing_Widget/ProcessingOptions/BilateralWidget.hpp
        Media_Widget/MediaProcessing_Widget/ProcessingOptions/BilateralWidget.ui
        Media_Widget/MediaProcessing_Widget/ProcessingOptions/ClaheWidget.cpp
        Media_Widget/MediaProcessing_Widget/ProcessingOptions/ClaheWidget.hpp
        Media_Widget/MediaProcessing_Widget/ProcessingOptions/ClaheWidget.ui
        Media_Widget/MediaProcessing_Widget/ProcessingOptions/MedianWidget.cpp
        Media_Widget/MediaProcessing_Widget/ProcessingOptions/MedianWidget.hpp
        Media_Widget/MediaProcessing_Widget/ProcessingOptions/MedianWidget.ui
        Media_Widget/MediaProcessing_Widget/ProcessingOptions/MagicEraserWidget.cpp
        Media_Widget/MediaProcessing_Widget/ProcessingOptions/MagicEraserWidget.hpp
        Media_Widget/MediaProcessing_Widget/ProcessingOptions/MagicEraserWidget.ui

        Media_Widget/MediaMask_Widget/MediaMask_Widget.hpp
        Media_Widget/MediaMask_Widget/MediaMask_Widget.cpp
        Media_Widget/MediaMask_Widget/MediaMask_Widget.ui
        Media_Widget/MediaMask_Widget/SelectionWidgets/MaskNoneSelectionWidget.cpp
        Media_Widget/MediaMask_Widget/SelectionWidgets/MaskNoneSelectionWidget.hpp
        Media_Widget/MediaMask_Widget/SelectionWidgets/MaskNoneSelectionWidget.ui
        Media_Widget/MediaMask_Widget/SelectionWidgets/MaskBrushSelectionWidget.cpp
        Media_Widget/MediaMask_Widget/SelectionWidgets/MaskBrushSelectionWidget.hpp
        Media_Widget/MediaMask_Widget/SelectionWidgets/MaskBrushSelectionWidget.ui
        Media_Widget/MediaMask_Widget/MaskDilationWidget/MaskDilationWidget.cpp
        Media_Widget/MediaMask_Widget/MaskDilationWidget/MaskDilationWidget.hpp
        Media_Widget/MediaMask_Widget/MaskDilationWidget/MaskDilationWidget.ui

        Media_Widget/MediaPoint_Widget/MediaPoint_Widget.hpp
        Media_Widget/MediaPoint_Widget/MediaPoint_Widget.cpp
        Media_Widget/MediaPoint_Widget/MediaPoint_Widget.ui

        Media_Widget/MediaTensor_Widget/MediaTensor_Widget.hpp
        Media_Widget/MediaTensor_Widget/MediaTensor_Widget.cpp
        Media_Widget/MediaTensor_Widget/MediaTensor_Widget.ui

        Media_Widget/MediaLine_Widget/MediaLine_Widget.hpp
        Media_Widget/MediaLine_Widget/MediaLine_Widget.cpp
        Media_Widget/MediaLine_Widget/MediaLine_Widget.ui
        Media_Widget/MediaLine_Widget/SelectionWidgets/LineNoneSelectionWidget.cpp
        Media_Widget/MediaLine_Widget/SelectionWidgets/LineNoneSelectionWidget.hpp
        Media_Widget/MediaLine_Widget/SelectionWidgets/LineNoneSelectionWidget.ui
        Media_Widget/MediaLine_Widget/SelectionWidgets/LineAddSelectionWidget.cpp
        Media_Widget/MediaLine_Widget/SelectionWidgets/LineAddSelectionWidget.hpp
        Media_Widget/MediaLine_Widget/SelectionWidgets/LineAddSelectionWidget.ui
        Media_Widget/MediaLine_Widget/SelectionWidgets/LineEraseSelectionWidget.cpp
        Media_Widget/MediaLine_Widget/SelectionWidgets/LineEraseSelectionWidget.hpp
        Media_Widget/MediaLine_Widget/SelectionWidgets/LineEraseSelectionWidget.ui
        Media_Widget/MediaLine_Widget/SelectionWidgets/LineSelectSelectionWidget.cpp
        Media_Widget/MediaLine_Widget/SelectionWidgets/LineSelectSelectionWidget.hpp
        Media_Widget/MediaLine_Widget/SelectionWidgets/LineSelectSelectionWidget.ui

        Media_Widget/MediaInterval_Widget/MediaInterval_Widget.hpp
        Media_Widget/MediaInterval_Widget/MediaInterval_Widget.cpp
        Media_Widget/MediaInterval_Widget/MediaInterval_Widget.ui
        Media_Widget/MediaInterval_Widget/StyleWidgets/BoxIntervalStyle_Widget.hpp
        Media_Widget/MediaInterval_Widget/StyleWidgets/BoxIntervalStyle_Widget.cpp
        Media_Widget/MediaInterval_Widget/StyleWidgets/BoxIntervalStyle_Widget.ui
        Media_Widget/MediaInterval_Widget/StyleWidgets/BorderIntervalStyle_Widget.hpp
        Media_Widget/MediaInterval_Widget/StyleWidgets/BorderIntervalStyle_Widget.cpp
        Media_Widget/MediaInterval_Widget/StyleWidgets/BorderIntervalStyle_Widget.ui

        Media_Widget/MediaText_Widget/MediaText_Widget/MediaText_Widget.hpp
        Media_Widget/MediaText_Widget/MediaText_Widget/MediaText_Widget.cpp
        Media_Widget/MediaText_Widget/MediaText_Widget/MediaText_Widget.ui

        Media_Widget/MouseBrush_Widget/MouseBrush_Widget.hpp
        Media_Widget/MouseBrush_Widget/MouseBrush_Widget.cpp
        Media_Widget/MouseBrush_Widget/MouseBrush_Widget.ui

        TimeScrollBar/TimeScrollBar.hpp
        TimeScrollBar/TimeScrollBar.cpp
        TimeScrollBar/TimeScrollBar.ui

        DataViewer_Widget/DataViewer_Widget.cpp
        DataViewer_Widget/DataViewer_Widget.hpp
        DataViewer_Widget/DataViewer_Widget.ui
        DataViewer_Widget/DataViewer_Tree_Widget/DataViewer_Tree_Widget.cpp
        DataViewer_Widget/DataViewer_Tree_Widget/DataViewer_Tree_Widget.hpp
        DataViewer_Widget/DataViewer_Tree_Widget/TreeWidgetStateManager.cpp
        DataViewer_Widget/DataViewer_Tree_Widget/TreeWidgetStateManager.hpp
        DataViewer_Widget/OpenGLWidget.cpp
        DataViewer_Widget/OpenGLWidget.hpp
        DataViewer_Widget/shaders/dashed_line_shader.hpp
        DataViewer_Widget/shaders/colored_vertex_shader.hpp
        DataViewer_Widget/AnalogTimeSeries/AnalogViewer_Widget.cpp
        DataViewer_Widget/AnalogTimeSeries/AnalogViewer_Widget.hpp
        DataViewer_Widget/AnalogTimeSeries/AnalogViewer_Widget.ui
        DataViewer_Widget/DigitalEvent/EventViewer_Widget.cpp
        DataViewer_Widget/DigitalEvent/EventViewer_Widget.hpp
        DataViewer_Widget/DigitalEvent/EventViewer_Widget.ui
        DataViewer_Widget/DigitalInterval/IntervalViewer_Widget.cpp
        DataViewer_Widget/DigitalInterval/IntervalViewer_Widget.hpp
        DataViewer_Widget/DigitalInterval/IntervalViewer_Widget.ui


        ML_Widget/ML_Widget.hpp
        ML_Widget/ML_Widget.cpp
        ML_Widget/ML_Widget.ui
        ML_Widget/mlpack_conversion.hpp
        ML_Widget/mlpack_conversion.cpp
        ML_Widget/MLModelOperation.hpp
        ML_Widget/MLModelRegistry.cpp
        ML_Widget/MLModelRegistry.hpp
        ML_Widget/MLModelParameters.hpp
        ML_Widget/MLParameterWidgetBase.hpp
        ML_Widget/Transformations/TransformationsCommon.hpp
        ML_Widget/Transformations/ITransformation.hpp
        ML_Widget/Transformations/TransformationBase.cpp
        ML_Widget/Transformations/TransformationBase.hpp
        ML_Widget/Transformations/IdentityTransform.cpp
        ML_Widget/Transformations/IdentityTransform.hpp
        ML_Widget/Transformations/SquaredTransform.cpp
        ML_Widget/Transformations/SquaredTransform.hpp
        ML_Widget/Transformations/LagLeadTransform.cpp
        ML_Widget/Transformations/LagLeadTransform.hpp
        ML_Widget/ClassBalancingWidget/ClassBalancingWidget.cpp
        ML_Widget/ClassBalancingWidget/ClassBalancingWidget.hpp
        ML_Widget/ClassBalancingWidget/ClassBalancingWidget.ui
        ML_Widget/FeatureProcessingWidget/FeatureProcessingWidget.cpp
        ML_Widget/FeatureProcessingWidget/FeatureProcessingWidget.hpp
        ML_Widget/FeatureProcessingWidget/FeatureProcessingWidget.ui
        ML_Widget/ModelMetricsWidget/ModelMetricsWidget.cpp
        ML_Widget/ModelMetricsWidget/ModelMetricsWidget.hpp
        ML_Widget/ModelMetricsWidget/ModelMetricsWidget.ui
        ML_Widget/ML_Random_Forest_Widget/ML_Random_Forest_Widget.hpp
        ML_Widget/ML_Random_Forest_Widget/ML_Random_Forest_Widget.cpp
        ML_Widget/ML_Random_Forest_Widget/ML_Random_Forest_Widget.ui
        ML_Widget/ML_Random_Forest_Widget/RandomForestModelOperation.hpp
        ML_Widget/ML_Random_Forest_Widget/RandomForestModelOperation.cpp
        ML_Widget/ML_Naive_Bayes_Widget/ML_Naive_Bayes_Widget.hpp
        ML_Widget/ML_Naive_Bayes_Widget/ML_Naive_Bayes_Widget.cpp
        ML_Widget/ML_Naive_Bayes_Widget/ML_Naive_Bayes_Widget.ui
        ML_Widget/ML_Naive_Bayes_Widget/NaiveBayesModelOperation.hpp
        ML_Widget/ML_Naive_Bayes_Widget/NaiveBayesModelOperation.cpp

        Tongue_Widget/Tongue_Widget.hpp
        Tongue_Widget/Tongue_Widget.cpp
        Tongue_Widget/Tongue_Widget.ui

        Feature_Table_Widget/Feature_Table_Widget.hpp
        Feature_Table_Widget/Feature_Table_Widget.cpp
        Feature_Table_Widget/Feature_Table_Widget.ui
        Color_Widget/Color_Widget.hpp

        Feature_Tree_Widget/Feature_Tree_Widget.cpp
        Feature_Tree_Widget/Feature_Tree_Widget.hpp
        Feature_Tree_Widget/Feature_Tree_Widget.ui

        Grabcut_Widget/Grabcut_Widget.hpp
        Grabcut_Widget/Grabcut_Widget.cpp
        Grabcut_Widget/GrabCutTool.cpp
        Grabcut_Widget/GrabCutTool.hpp
        Grabcut_Widget/Grabcut_Widget.ui

        Collapsible_Widget/Section.cpp
        Collapsible_Widget/Section.hpp

        Magic_Eraser_Widget/magic_eraser.hpp
        Magic_Eraser_Widget/magic_eraser.cpp

        Export_Widgets/Export_Video_Widget/Export_Video_Widget.hpp
        Export_Widgets/Export_Video_Widget/Export_Video_Widget.cpp
        Export_Widgets/Export_Video_Widget/Export_Video_Widget.ui

        IO_Widgets/Scaling_Widget/Scaling_Widget.cpp
        IO_Widgets/Scaling_Widget/Scaling_Widget.hpp
        IO_Widgets/Scaling_Widget/Scaling_Widget.ui

        IO_Widgets/DataAggregation/DataAggregationExporter_Widget.cpp
        IO_Widgets/DataAggregation/DataAggregationExporter_Widget.hpp
        IO_Widgets/DataAggregation/DataAggregationExporter_Widget.ui

        IO_Widgets/AnalogTimeSeries/Analog_Loader_Widget.hpp
        IO_Widgets/AnalogTimeSeries/Analog_Loader_Widget.cpp
        IO_Widgets/AnalogTimeSeries/Analog_Loader_Widget.ui
        IO_Widgets/AnalogTimeSeries/CSV/CSVAnalogLoader_Widget.cpp
        IO_Widgets/AnalogTimeSeries/CSV/CSVAnalogLoader_Widget.hpp
        IO_Widgets/AnalogTimeSeries/CSV/CSVAnalogLoader_Widget.ui
        IO_Widgets/AnalogTimeSeries/CSV/CSVAnalogSaver_Widget.cpp
        IO_Widgets/AnalogTimeSeries/CSV/CSVAnalogSaver_Widget.hpp
        IO_Widgets/AnalogTimeSeries/CSV/CSVAnalogSaver_Widget.ui
        IO_Widgets/AnalogTimeSeries/Binary/BinaryAnalogLoader_Widget.cpp
        IO_Widgets/AnalogTimeSeries/Binary/BinaryAnalogLoader_Widget.hpp
        IO_Widgets/AnalogTimeSeries/Binary/BinaryAnalogLoader_Widget.ui

        IO_Widgets/Points/Point_Loader_Widget.hpp
        IO_Widgets/Points/Point_Loader_Widget.cpp
        IO_Widgets/Points/Point_Loader_Widget.ui
        IO_Widgets/Points/CSV/CSVPointLoader_Widget.cpp
        IO_Widgets/Points/CSV/CSVPointLoader_Widget.hpp
        IO_Widgets/Points/CSV/CSVPointLoader_Widget.ui
        IO_Widgets/Points/CSV/CSVPointSaver_Widget.cpp
        IO_Widgets/Points/CSV/CSVPointSaver_Widget.hpp
        IO_Widgets/Points/CSV/CSVPointSaver_Widget.ui

        IO_Widgets/Masks/Mask_Loader_Widget.hpp
        IO_Widgets/Masks/Mask_Loader_Widget.cpp
        IO_Widgets/Masks/Mask_Loader_Widget.ui
        IO_Widgets/Masks/HDF5/HDF5MaskLoader_Widget.cpp
        IO_Widgets/Masks/HDF5/HDF5MaskLoader_Widget.hpp
        IO_Widgets/Masks/HDF5/HDF5MaskLoader_Widget.ui
        IO_Widgets/Masks/HDF5/HDF5MaskSaver_Widget.cpp
        IO_Widgets/Masks/HDF5/HDF5MaskSaver_Widget.hpp
        IO_Widgets/Masks/HDF5/HDF5MaskSaver_Widget.ui
        IO_Widgets/Masks/Image/ImageMaskLoader_Widget.cpp
        IO_Widgets/Masks/Image/ImageMaskLoader_Widget.hpp
        IO_Widgets/Masks/Image/ImageMaskLoader_Widget.ui
        IO_Widgets/Masks/Image/ImageMaskSaver_Widget.cpp
        IO_Widgets/Masks/Image/ImageMaskSaver_Widget.hpp
        IO_Widgets/Masks/Image/ImageMaskSaver_Widget.ui

        IO_Widgets/Lines/Line_Loader_Widget.hpp
        IO_Widgets/Lines/Line_Loader_Widget.cpp
        IO_Widgets/Lines/Line_Loader_Widget.ui
        IO_Widgets/Lines/HDF5/HDF5LineLoader_Widget.hpp
        IO_Widgets/Lines/HDF5/HDF5LineLoader_Widget.cpp
        IO_Widgets/Lines/HDF5/HDF5LineLoader_Widget.ui
        IO_Widgets/Lines/CSV/CSVLineLoader_Widget.hpp
        IO_Widgets/Lines/CSV/CSVLineLoader_Widget.cpp
        IO_Widgets/Lines/CSV/CSVLineLoader_Widget.ui
        IO_Widgets/Lines/CSV/CSVLineSaver_Widget.cpp
        IO_Widgets/Lines/CSV/CSVLineSaver_Widget.hpp
        IO_Widgets/Lines/CSV/CSVLineSaver_Widget.ui
        IO_Widgets/Lines/Binary/BinaryLineSaver_Widget.cpp
        IO_Widgets/Lines/Binary/BinaryLineSaver_Widget.hpp
        IO_Widgets/Lines/Binary/BinaryLineSaver_Widget.ui
        IO_Widgets/Lines/Binary/BinaryLineLoader_Widget.cpp
        IO_Widgets/Lines/Binary/BinaryLineLoader_Widget.hpp
        IO_Widgets/Lines/Binary/BinaryLineLoader_Widget.ui
        IO_Widgets/Lines/LMDB/LMDBLineLoader_Widget.hpp
        IO_Widgets/Lines/LMDB/LMDBLineLoader_Widget.cpp
        IO_Widgets/Lines/LMDB/LMDBLineLoader_Widget.ui

        IO_Widgets/DigitalTimeSeries/Digital_Interval_Loader_Widget.hpp
        IO_Widgets/DigitalTimeSeries/Digital_Interval_Loader_Widget.cpp
        IO_Widgets/DigitalTimeSeries/Digital_Interval_Loader_Widget.ui
        IO_Widgets/DigitalTimeSeries/CSV/CSVDigitalIntervalLoader_Widget.cpp
        IO_Widgets/DigitalTimeSeries/CSV/CSVDigitalIntervalLoader_Widget.hpp
        IO_Widgets/DigitalTimeSeries/CSV/CSVDigitalIntervalLoader_Widget.ui
        IO_Widgets/DigitalTimeSeries/Binary/BinaryDigitalIntervalLoader_Widget.cpp
        IO_Widgets/DigitalTimeSeries/Binary/BinaryDigitalIntervalLoader_Widget.hpp
        IO_Widgets/DigitalTimeSeries/Binary/BinaryDigitalIntervalLoader_Widget.ui
        IO_Widgets/DigitalTimeSeries/CSV/CSVIntervalSaver_Widget.cpp
        IO_Widgets/DigitalTimeSeries/CSV/CSVIntervalSaver_Widget.hpp
        IO_Widgets/DigitalTimeSeries/CSV/CSVIntervalSaver_Widget.ui

        IO_Widgets/DigitalTimeSeries/Digital_Event_Loader_Widget.cpp
        IO_Widgets/DigitalTimeSeries/Digital_Event_Loader_Widget.hpp
        IO_Widgets/DigitalTimeSeries/Digital_Event_Loader_Widget.ui
        IO_Widgets/DigitalTimeSeries/CSV/CSVDigitalEventLoader_Widget.cpp
        IO_Widgets/DigitalTimeSeries/CSV/CSVDigitalEventLoader_Widget.hpp
        IO_Widgets/DigitalTimeSeries/CSV/CSVDigitalEventLoader_Widget.ui

        IO_Widgets/Tensors/Tensor_Loader_Widget.hpp
        IO_Widgets/Tensors/Tensor_Loader_Widget.cpp
        IO_Widgets/Tensors/Tensor_Loader_Widget.ui

        IO_Widgets/Media/media_export.hpp
        IO_Widgets/Media/media_export.cpp
        IO_Widgets/Media/MediaExport_Widget.cpp
        IO_Widgets/Media/MediaExport_Widget.hpp
        IO_Widgets/Media/MediaExport_Widget.ui

        utils/qt_utilities.hpp
        utils/qt_utilities.cpp

        utils/Deep_Learning/Backbones/efficientvit.hpp
        utils/Deep_Learning/Backbones/efficientvit.cpp

        utils/Deep_Learning/torch_helpers.hpp

        utils/Deep_Learning/scm.hpp
        utils/Deep_Learning/scm.cpp

        utils/Deep_Learning/Models/EfficientSAM/EfficientSAM.hpp
        utils/Deep_Learning/Models/EfficientSAM/EfficientSAM.cpp
)

if (WIN32)

#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /fsanitize=address")
endif()

qt_add_executable(WhiskerToolbox
        ${PROJECT_SOURCES}
)

target_link_libraries(WhiskerToolbox PRIVATE Qt6::Widgets Qt6::PrintSupport Qt6::Xml Qt6::OpenGLWidgets Qt6::OpenGL)

target_link_libraries(WhiskerToolbox PRIVATE HighFive)
target_link_libraries(WhiskerToolbox PRIVATE nlohmann_json::nlohmann_json)

target_link_libraries(WhiskerToolbox PRIVATE glm::glm)

find_path(MLPACK_INCLUDE_DIRS "mlpack.hpp")

message(STATUS ${TORCH_LIBRARIES})
target_link_libraries(WhiskerToolbox PRIVATE "${TORCH_LIBRARIES}")

message(STATUS "Find MLPack include DIRS")
message(STATUS ${MLPACK_INCLUDE_DIRS})

target_include_directories(WhiskerToolbox PRIVATE

  #      "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/AnalogViewer>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/DataManager>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/DataViewer>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Label_Widget>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Main_Window>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Media_Window>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Whisker_Widget>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Grabcut_Widget>"

        ${MLPACK_INCLUDE_DIRS}

        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(WhiskerToolbox PRIVATE Whisker-Analysis)
target_link_libraries(WhiskerToolbox PRIVATE DataManager DataViewer)

target_link_libraries(WhiskerToolbox PRIVATE opencv_core opencv_imgproc opencv_highgui opencv_photo)

target_link_libraries(WhiskerToolbox PRIVATE armadillo)
target_link_libraries(WhiskerToolbox PRIVATE LAPACK::LAPACK)
target_link_libraries(WhiskerToolbox PRIVATE OpenBLAS::OpenBLAS)

#target_link_libraries(WhiskerToolbox PRIVATE JKQTPlotter6::JKQTPlotter6)

target_link_libraries(WhiskerToolbox PRIVATE qt6advanceddocking)

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
