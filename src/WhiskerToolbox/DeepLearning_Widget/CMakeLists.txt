find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(DEEP_LEARNING_WIDGET_SOURCES
    DeepLearningWidgetRegistration.hpp
    DeepLearningWidgetRegistration.cpp

    Core/BatchInferenceResult.hpp
    Core/DeepLearningBindingData.hpp
    Core/DeepLearningParamSchemas.hpp
    Core/DeepLearningParamSchemas.cpp
    Core/DeepLearningState.hpp
    Core/DeepLearningState.cpp
    Core/SlotAssembler.hpp
    Core/SlotAssembler.cpp

    UI/DeepLearningViewWidget.hpp
    UI/DeepLearningViewWidget.cpp

    UI/DeepLearningPropertiesWidget.hpp
    UI/DeepLearningPropertiesWidget.cpp

    UI/Helpers/DataSourceComboHelper.hpp
    UI/Helpers/DataSourceComboHelper.cpp
    UI/Helpers/DynamicInputSlotWidget.hpp
    UI/Helpers/DynamicInputSlotWidget.cpp
    UI/Helpers/StaticInputSlotWidget.hpp
    UI/Helpers/StaticInputSlotWidget.cpp
    UI/Helpers/SlotBindingWidget.hpp
    UI/Helpers/SlotBindingWidget.cpp
)

add_library(DeepLearning_Widget STATIC ${DEEP_LEARNING_WIDGET_SOURCES})

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

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(DeepLearning_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DeepLearning_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(DeepLearning_Widget PRIVATE ${MSVC_WARNINGS})
endif()

enable_whiskertoolbox_shared_pch(DeepLearning_Widget)