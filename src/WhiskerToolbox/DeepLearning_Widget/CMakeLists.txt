find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(DEEP_LEARNING_WIDGET_SOURCES
    DeepLearningWidgetRegistration.hpp
    DeepLearningWidgetRegistration.cpp

    Core/DeepLearningState.hpp
    Core/DeepLearningState.cpp
    Core/DeepLearningBindingData.hpp
    Core/SlotAssembler.hpp
    Core/SlotAssembler.cpp

    UI/DeepLearningViewWidget.hpp
    UI/DeepLearningViewWidget.cpp

    UI/DeepLearningPropertiesWidget.hpp
    UI/DeepLearningPropertiesWidget.cpp
)

add_library(DeepLearning_Widget STATIC ${DEEP_LEARNING_WIDGET_SOURCES})

target_link_libraries(DeepLearning_Widget PRIVATE Qt6::Widgets)
target_link_libraries(DeepLearning_Widget PRIVATE EditorState)
target_link_libraries(DeepLearning_Widget PRIVATE DataManager)
target_link_libraries(DeepLearning_Widget PRIVATE DeepLearning)

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

