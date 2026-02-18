find_package(reflectcpp CONFIG REQUIRED)


set(TRANSFORM_V2_SOURCES
    detail/ContainerTraits.hpp
    detail/ContainerTraits.cpp 

        core/DataManagerIntegration.hpp
        core/DataManagerIntegration.cpp
        core/ElementRegistry.hpp 
        core/ParameterSchema.hpp
        core/ParameterSchema.cpp
        core/PipelineLoader.hpp
        core/PipelineLoader.cpp
        core/PipelineValueStore.hpp
        core/RangeReductionRegistry.hpp
        core/RegisteredTransforms.cpp
        core/TensorColumnBuilders.hpp
        core/TensorColumnBuilders.cpp
        core/TransformPipeline.hpp
        core/TransformPipeline.cpp

        extension/ElementTransform.hpp
        extension/RangeReductionTypes.hpp
        extension/ValueProjectionTypes.hpp
        extension/ViewAdaptorTypes.hpp

        
        algorithms/AnalogEventThreshold/AnalogEventThreshold.hpp
        algorithms/AnalogEventThreshold/AnalogEventThreshold.cpp
        algorithms/AnalogIntervalThreshold/AnalogIntervalThreshold.hpp
        algorithms/AnalogIntervalThreshold/AnalogIntervalThreshold.cpp
        algorithms/AnalogIntervalPeak/AnalogIntervalPeak.hpp
        algorithms/AnalogIntervalPeak/AnalogIntervalPeak.cpp
        algorithms/LineMinPointDist/LineMinPointDist.hpp
        algorithms/LineMinPointDist/LineMinPointDist.cpp
        algorithms/MaskArea/MaskArea.hpp
        algorithms/MaskArea/MaskArea.cpp
        algorithms/SumReduction/SumReduction.hpp
        algorithms/SumReduction/SumReduction.cpp
        algorithms/AnalogHilbertPhase/AnalogHilbertPhase.hpp
        algorithms/AnalogHilbertPhase/AnalogHilbertPhase.cpp
        algorithms/DigitalIntervalBoolean/DigitalIntervalBoolean.hpp
        algorithms/DigitalIntervalBoolean/DigitalIntervalBoolean.cpp
        algorithms/LineAngle/LineAngle.hpp
        algorithms/LineAngle/LineAngle.cpp
        algorithms/LineBaseFlip/LineBaseFlip.hpp
        algorithms/LineBaseFlip/LineBaseFlip.cpp
        algorithms/LineClip/LineClip.hpp
        algorithms/LineClip/LineClip.cpp
        algorithms/LineCurvature/LineCurvature.hpp
        algorithms/LineCurvature/LineCurvature.cpp
        algorithms/LineLength/LineLength.hpp
        algorithms/LineLength/LineLength.cpp
        algorithms/LineResample/LineResample.hpp
        algorithms/LineResample/LineResample.cpp
        algorithms/LineSubsegment/LineSubsegment.hpp
        algorithms/LineSubsegment/LineSubsegment.cpp
        algorithms/LinePointExtraction/LinePointExtraction.hpp
        algorithms/LinePointExtraction/LinePointExtraction.cpp
        algorithms/MaskCentroid/MaskCentroid.hpp
        algorithms/MaskCentroid/MaskCentroid.cpp
        algorithms/RangeReductions/EventRangeReductions.hpp
        algorithms/RangeReductions/IntervalRangeReductions.hpp
        algorithms/RangeReductions/ValueRangeReductions.hpp
        algorithms/RangeReductions/RegisteredRangeReductions.hpp
        algorithms/RangeReductions/RegisteredRangeReductions.cpp
        algorithms/Temporal/NormalizeTime.hpp
        algorithms/Temporal/RegisteredTemporalTransforms.hpp
        algorithms/Temporal/RegisteredTemporalTransforms.cpp
)

add_library(TransformsV2 STATIC ${TRANSFORM_V2_SOURCES})

set_target_properties(TransformsV2
    PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)

target_include_directories(TransformsV2 PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )

target_include_directories(TransformsV2 PRIVATE
        "${iir_SOURCE_DIR}"
    )

target_link_libraries(TransformsV2 PUBLIC NEURALYZER_GEOMETRY)
target_link_libraries(TransformsV2 PUBLIC WhiskerToolbox::TimeFrame)
target_link_libraries(TransformsV2 PUBLIC WhiskerToolbox::Entity)

target_link_libraries(TransformsV2 PUBLIC reflectcpp::reflectcpp)

target_link_libraries(DataManager PRIVATE WhiskerToolbox::TensorData)
target_link_libraries(DataManager PUBLIC WhiskerToolbox::LineData)
target_link_libraries(DataManager PUBLIC WhiskerToolbox::MaskData)
target_link_libraries(DataManager PUBLIC WhiskerToolbox::MediaData)
target_link_libraries(DataManager PUBLIC WhiskerToolbox::AnalogTimeSeries)
target_link_libraries(DataManager PUBLIC WhiskerToolbox::DigitalTimeSeries)
target_link_libraries(DataManager PUBLIC WhiskerToolbox::PointData)

target_link_libraries(TransformsV2 PUBLIC DataManager)