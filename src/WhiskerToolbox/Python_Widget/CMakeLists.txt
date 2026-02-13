find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)
find_package(reflectcpp CONFIG REQUIRED)

set(PYTHON_WIDGET_SOURCES
    PythonWidgetRegistration.hpp
    PythonWidgetRegistration.cpp

    Core/PythonWidgetState.hpp
    Core/PythonWidgetState.cpp
    Core/PythonWidgetStateData.hpp

    UI/PythonViewWidget.hpp
    UI/PythonViewWidget.cpp
    UI/PythonViewWidget.ui

    UI/PythonPropertiesWidget.hpp
    UI/PythonPropertiesWidget.cpp
    UI/PythonPropertiesWidget.ui
)

add_library(Python_Widget STATIC ${PYTHON_WIDGET_SOURCES})

target_link_libraries(Python_Widget
    PUBLIC  EditorState
    PRIVATE Qt6::Widgets Qt6::Core Qt6::Gui reflectcpp::reflectcpp
)

target_include_directories(Python_Widget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_compile_definitions(Python_Widget PRIVATE _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(Python_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(Python_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(Python_Widget PRIVATE ${MSVC_WARNINGS})
endif()

set_target_properties(Python_Widget PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)
