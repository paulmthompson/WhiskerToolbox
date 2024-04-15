
add_library(Analog_Viewer SHARED
        analog_viewer.hpp
        analog_viewer.cpp
)

target_include_directories(Analog_Viewer PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>")