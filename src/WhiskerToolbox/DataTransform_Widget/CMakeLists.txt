

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(DATA_TRANSFORM_WIDGET_SOURCES
        DataTransform_Widget.ui
        DataTransform_Widget.cpp
        DataTransform_Widget.hpp
        DataTransformWidgetState.hpp
        DataTransformWidgetState.cpp
        TransformParameter_Widget/TransformParameter_Widget.hpp
        TransformParameter_Widget/DataManagerParameter_Widget.hpp
        TransformParameter_Widget/DataManagerParameter_Widget.cpp

        Masks/MaskArea_Widget/MaskArea_Widget.hpp
        Masks/MaskArea_Widget/MaskArea_Widget.cpp
        Masks/MaskArea_Widget/MaskArea_Widget.ui
        Masks/MaskCentroid_Widget/MaskCentroid_Widget.hpp
        Masks/MaskCentroid_Widget/MaskCentroid_Widget.cpp
        Masks/MaskCentroid_Widget/MaskCentroid_Widget.ui
        Masks/MaskConnectedComponent_Widget/MaskConnectedComponent_Widget.hpp
        Masks/MaskConnectedComponent_Widget/MaskConnectedComponent_Widget.cpp
        Masks/MaskConnectedComponent_Widget/MaskConnectedComponent_Widget.ui
        Masks/MaskHoleFilling_Widget/MaskHoleFilling_Widget.hpp
        Masks/MaskHoleFilling_Widget/MaskHoleFilling_Widget.cpp
        Masks/MaskHoleFilling_Widget/MaskHoleFilling_Widget.ui
        Masks/MaskMedianFilter_Widget/MaskMedianFilter_Widget.hpp
        Masks/MaskMedianFilter_Widget/MaskMedianFilter_Widget.cpp
        Masks/MaskMedianFilter_Widget/MaskMedianFilter_Widget.ui
        Masks/MaskPrincipalAxis_Widget/MaskPrincipalAxis_Widget.hpp
        Masks/MaskPrincipalAxis_Widget/MaskPrincipalAxis_Widget.cpp
        Masks/MaskPrincipalAxis_Widget/MaskPrincipalAxis_Widget.ui
        Masks/MaskToLine_Widget/MaskToLine_Widget.cpp
        Masks/MaskToLine_Widget/MaskToLine_Widget.hpp
        Masks/MaskToLine_Widget/MaskToLine_Widget.ui
        Masks/MaskSkeletonize_Widget/MaskSkeletonize_Widget.cpp
        Masks/MaskSkeletonize_Widget/MaskSkeletonize_Widget.hpp
        Masks/MaskSkeletonize_Widget/MaskSkeletonize_Widget.ui
        AnalogTimeSeries/AnalogEventThreshold_Widget/AnalogEventThreshold_Widget.ui
        AnalogTimeSeries/AnalogEventThreshold_Widget/AnalogEventThreshold_Widget.hpp
        AnalogTimeSeries/AnalogEventThreshold_Widget/AnalogEventThreshold_Widget.cpp
        AnalogTimeSeries/AnalogFilter_Widget/AnalogFilter_Widget.hpp
        AnalogTimeSeries/AnalogFilter_Widget/AnalogFilter_Widget.cpp
        AnalogTimeSeries/AnalogFilter_Widget/AnalogFilter_Widget.ui
        AnalogTimeSeries/AnalogIntervalPeak_Widget/AnalogIntervalPeak_Widget.hpp
        AnalogTimeSeries/AnalogIntervalPeak_Widget/AnalogIntervalPeak_Widget.cpp
        AnalogTimeSeries/AnalogIntervalPeak_Widget/AnalogIntervalPeak_Widget.ui
        AnalogTimeSeries/AnalogIntervalThreshold_Widget/AnalogIntervalThreshold_Widget.hpp
        AnalogTimeSeries/AnalogIntervalThreshold_Widget/AnalogIntervalThreshold_Widget.cpp
        AnalogTimeSeries/AnalogIntervalThreshold_Widget/AnalogIntervalThreshold_Widget.ui
        AnalogTimeSeries/AnalogHilbertPhase_Widget/AnalogHilbertPhase_Widget.hpp
        AnalogTimeSeries/AnalogHilbertPhase_Widget/AnalogHilbertPhase_Widget.cpp
        AnalogTimeSeries/AnalogHilbertPhase_Widget/AnalogHilbertPhase_Widget.ui
        AnalogTimeSeries/AnalogScaling_Widget/AnalogScaling_Widget.cpp
        AnalogTimeSeries/AnalogScaling_Widget/AnalogScaling_Widget.hpp
        AnalogTimeSeries/AnalogScaling_Widget/AnalogScaling_Widget.ui
        Lines/LineAngle_Widget/LineAngle_Widget.hpp
        Lines/LineAngle_Widget/LineAngle_Widget.cpp
        Lines/LineAngle_Widget/LineAngle_Widget.ui
        Lines/LineMinDist_Widget/LineMinDist_Widget.hpp
        Lines/LineMinDist_Widget/LineMinDist_Widget.cpp
        Lines/LineMinDist_Widget/LineMinDist_Widget.ui
        Lines/LineAlignment_Widget/LineAlignment_Widget.hpp
        Lines/LineAlignment_Widget/LineAlignment_Widget.cpp
        Lines/LineAlignment_Widget/LineAlignment_Widget.ui
        Lines/LineBaseFlip_Widget/LineBaseFlip_Widget.hpp
        Lines/LineBaseFlip_Widget/LineBaseFlip_Widget.cpp
        Lines/LineBaseFlip_Widget/LineBaseFlip_Widget.ui
        Lines/LineResample_Widget/LineResample_Widget.cpp
        Lines/LineResample_Widget/LineResample_Widget.hpp
        Lines/LineResample_Widget/LineResample_Widget.ui
        Lines/LineCurvature_Widget/LineCurvature_Widget.cpp
        Lines/LineCurvature_Widget/LineCurvature_Widget.hpp
        Lines/LineCurvature_Widget/LineCurvature_Widget.ui
        Lines/LineSubsegment_Widget/LineSubsegment_Widget.cpp
        Lines/LineSubsegment_Widget/LineSubsegment_Widget.hpp
        Lines/LineSubsegment_Widget/LineSubsegment_Widget.ui
        Lines/LinePointExtraction_Widget/LinePointExtraction_Widget.cpp
        Lines/LinePointExtraction_Widget/LinePointExtraction_Widget.hpp
        Lines/LinePointExtraction_Widget/LinePointExtraction_Widget.ui
        Lines/LineClip_Widget/LineClip_Widget.cpp
        Lines/LineClip_Widget/LineClip_Widget.hpp
        Lines/LineClip_Widget/LineClip_Widget.ui
        Lines/Line_Proximity_Grouping/LineProximityGrouping_Widget.cpp
        Lines/Line_Proximity_Grouping/LineProximityGrouping_Widget.hpp
        Lines/Line_Proximity_Grouping/LineProximityGrouping_Widget.ui
        Lines/Line_Kalman_Grouping/LineKalmanGrouping_Widget.cpp
        Lines/Line_Kalman_Grouping/LineKalmanGrouping_Widget.hpp
        Lines/Line_Kalman_Grouping/LineKalmanGrouping_Widget.ui
        Lines/Line_Outlier_Detection/LineOutlierDetection_Widget.cpp
        Lines/Line_Outlier_Detection/LineOutlierDetection_Widget.hpp
        Lines/Line_Outlier_Detection/LineOutlierDetection_Widget.ui
        Lines/LineIndexGrouping_Widget/LineIndexGrouping_Widget.cpp
        Lines/LineIndexGrouping_Widget/LineIndexGrouping_Widget.hpp
        Lines/LineIndexGrouping_Widget/LineIndexGrouping_Widget.ui
        Lines/LineGroupToIntervals_Widget/LineGroupToIntervals_Widget.cpp
        Lines/LineGroupToIntervals_Widget/LineGroupToIntervals_Widget.hpp
        Lines/LineGroupToIntervals_Widget/LineGroupToIntervals_Widget.ui
        Points/PointParticleFilter_Widget/PointParticleFilter_Widget.cpp
        Points/PointParticleFilter_Widget/PointParticleFilter_Widget.hpp
        Points/PointParticleFilter_Widget/PointParticleFilter_Widget.ui
        DigitalIntervalSeries/GroupIntervals_Widget/GroupIntervals_Widget.cpp
        DigitalIntervalSeries/GroupIntervals_Widget/GroupIntervals_Widget.hpp
        DigitalIntervalSeries/GroupIntervals_Widget/GroupIntervals_Widget.ui
        DigitalIntervalSeries/BooleanOperation_Widget/BooleanOperation_Widget.cpp
        DigitalIntervalSeries/BooleanOperation_Widget/BooleanOperation_Widget.hpp
        DigitalIntervalSeries/BooleanOperation_Widget/BooleanOperation_Widget.ui
        Media/WhiskerTracing_Widget/WhiskerTracing_Widget.hpp
        Media/WhiskerTracing_Widget/WhiskerTracing_Widget.cpp
        Media/WhiskerTracing_Widget/WhiskerTracing_Widget.ui
)

add_library(DataTransform_Widget STATIC ${DATA_TRANSFORM_WIDGET_SOURCES})

set_target_properties(DataTransform_Widget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(DataTransform_Widget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
)

target_link_libraries(DataTransform_Widget PRIVATE DataManager)
target_link_libraries(DataTransform_Widget PRIVATE WhiskerToolbox::EditorState)
target_link_libraries(DataTransform_Widget PRIVATE NEURALYZER_GEOMETRY)

# Include directories
target_include_directories(DataTransform_Widget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../DataManager>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compile definitions
target_compile_definitions(DataTransform_Widget PRIVATE _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(DataTransform_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DataTransform_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(DataTransform_Widget PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(DataTransform_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
