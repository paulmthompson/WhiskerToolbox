find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(TRIAGE_SESSION_WIDGET_SOURCES
        Core/TriageSessionState.hpp
        Core/TriageSessionState.cpp
        UI/CommandPickerDialog.hpp
        UI/CommandPickerDialog.cpp
        UI/CommandRowWidget.hpp
        UI/CommandRowWidget.cpp
        UI/GuidedPipelineEditor.hpp
        UI/GuidedPipelineEditor.cpp
        UI/TriageSessionProperties_Widget.hpp
        UI/TriageSessionProperties_Widget.cpp
        TriageSessionWidgetRegistration.hpp
        TriageSessionWidgetRegistration.cpp
)

qt_add_library(TriageSession_Widget STATIC ${TRIAGE_SESSION_WIDGET_SOURCES})

target_include_directories(TriageSession_Widget PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
)

target_link_libraries(TriageSession_Widget PUBLIC
        Qt6::Widgets
        Qt6::Core
        Qt6::Gui
        AutoParamWidget
        EditorState
        DataManager
        StateManagement
        TriageSession
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(TriageSession_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(TriageSession_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(TriageSession_Widget PRIVATE ${MSVC_WARNINGS})
endif()

enable_whiskertoolbox_shared_pch(TriageSession_Widget)