

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_library(Analog_Viewer SHARED
        analog_viewer.hpp
        analog_viewer.cpp
        analog_viewer.ui
)

target_include_directories(Analog_Viewer PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>")

target_link_libraries(Analog_Viewer PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
#target_link_libraries(Analog_Viewer PRIVATE Qt${QT_VERSION_MAJOR}::Multimedia)
#target_link_libraries(Analog_Viewer PRIVATE Qt${QT_VERSION_MAJOR}::MultimediaWidgets)
#target_link_libraries(Analog_Viewer PRIVATE Qt${QT_VERSION_MAJOR}::OpenGL)
#target_link_libraries(Analog_Viewer PRIVATE Qt${QT_VERSION_MAJOR}::UiTools)

target_link_libraries(Analog_Viewer PRIVATE DataManager)
