find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(DEEP_LEARNING_WIDGET_SOURCES
    DeepLearningWidgetRegistration.hpp
    DeepLearningWidgetRegistration.cpp

    Core/BindingConversion.hpp
    Core/BindingConversion.cpp
    Core/ConstraintEnforcer.hpp
    Core/ConstraintEnforcer.cpp
    Core/DeepLearningBindingData.hpp
    Core/DeepLearningParamSchemas.hpp
    Core/DeepLearningParamSchemas.cpp
    Core/DeepLearningParamSchemasUIHints.hpp
    Core/DeepLearningState.hpp
    Core/DeepLearningState.cpp
    Core/SlotAssembler.hpp
    Core/SlotAssembler.cpp

    Inference/BatchInferenceResult.hpp
    Inference/BatchInferenceWorker.hpp
    Inference/BatchInferenceWorker.cpp
    Inference/InferenceController.hpp
    Inference/InferenceController.cpp
    Inference/MultiIntervalBatchWorker.hpp
    Inference/MultiIntervalBatchWorker.cpp
    Inference/ResultProcessor.hpp
    Inference/ResultProcessor.cpp

    UI/DeepLearningViewWidget.hpp
    UI/DeepLearningViewWidget.cpp
    UI/DeepLearningPropertiesWidget.hpp
    UI/DeepLearningPropertiesWidget.cpp

    UI/Helpers/DataSourceComboHelper.hpp
    UI/Helpers/DataSourceComboHelper.cpp
    UI/Helpers/DynamicInputSlotWidget.hpp
    UI/Helpers/DynamicInputSlotWidget.cpp
    UI/Helpers/EncoderShapeWidget.hpp
    UI/Helpers/EncoderShapeWidget.cpp
    UI/Helpers/OutputSlotWidget.hpp
    UI/Helpers/OutputSlotWidget.cpp
    UI/Helpers/OutputPipelineWidget.hpp
    UI/Helpers/OutputPipelineWidget.cpp
    UI/Helpers/OutputPipelineWidget.ui
    UI/Helpers/PostEncoderWidget.hpp
    UI/Helpers/PostEncoderWidget.cpp
    UI/Helpers/RecurrentBindingWidget.hpp
    UI/Helpers/RecurrentBindingWidget.cpp
    UI/Helpers/SequenceSlotWidget.hpp
    UI/Helpers/SequenceSlotWidget.cpp
    UI/Helpers/StaticInputSlotWidget.hpp
    UI/Helpers/StaticInputSlotWidget.cpp
)

add_library(DeepLearning_Widget STATIC ${DEEP_LEARNING_WIDGET_SOURCES})

set_target_properties(DeepLearning_Widget PROPERTIES
    AUTOUIC ON
    AUTOMOC ON
)

target_link_libraries(DeepLearning_Widget PUBLIC Qt6::Widgets)
target_link_libraries(DeepLearning_Widget PUBLIC EditorState)

target_link_libraries(DeepLearning_Widget PRIVATE DataManager)
target_link_libraries(DeepLearning_Widget PRIVATE WhiskerToolbox::PointData)
target_link_libraries(DeepLearning_Widget PRIVATE WhiskerToolbox::LineData)
target_link_libraries(DeepLearning_Widget PRIVATE WhiskerToolbox::MaskData)
target_link_libraries(DeepLearning_Widget PRIVATE WhiskerToolbox::MediaData)
target_link_libraries(DeepLearning_Widget PRIVATE WhiskerToolbox::AnalogTimeSeries)
target_link_libraries(DeepLearning_Widget PRIVATE WhiskerToolbox::DigitalTimeSeries)
target_link_libraries(DeepLearning_Widget PRIVATE WhiskerToolbox::TensorData)

target_link_libraries(DeepLearning_Widget PRIVATE DeepLearning)
target_link_libraries(DeepLearning_Widget PRIVATE WhiskerToolbox::ParameterSchema)
target_link_libraries(DeepLearning_Widget PRIVATE WhiskerToolbox::StateManagement)
target_link_libraries(DeepLearning_Widget PRIVATE AutoParamWidget)

target_include_directories(DeepLearning_Widget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_compiler_warnings(DeepLearning_Widget)

enable_whiskertoolbox_shared_pch(DeepLearning_Widget)