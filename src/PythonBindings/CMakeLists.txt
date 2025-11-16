find_package(pybind11 CONFIG REQUIRED)

# Find Python
find_package(Python COMPONENTS Interpreter Development REQUIRED)

# Create the Python module
pybind11_add_module(whiskertoolbox
    whiskertoolbox_module.cpp
    interval_bindings.cpp
    digital_interval_series_bindings.cpp
    transform_bindings.cpp
)

# Link against required libraries
target_link_libraries(whiskertoolbox PRIVATE
    TimeFrame
    DataManager
)

# Set module properties
set_target_properties(whiskertoolbox PROPERTIES
    CXX_VISIBILITY_PRESET "hidden"
    INTERPROCEDURAL_OPTIMIZATION TRUE
    PREFIX "${PYTHON_MODULE_PREFIX}"
    SUFFIX "${PYTHON_MODULE_EXTENSION}"
)

# Install the module to a Python-findable location
install(TARGETS whiskertoolbox
    LIBRARY DESTINATION ${Python_SITELIB}
    RUNTIME DESTINATION ${Python_SITELIB}
)
