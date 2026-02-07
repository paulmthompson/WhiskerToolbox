find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

pybind11_add_module(whiskertoolbox_python bindings.cpp)

target_link_libraries(whiskertoolbox_python PRIVATE WhiskerToolbox::DigitalTimeSeries)
