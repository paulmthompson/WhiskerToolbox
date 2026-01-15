# Find Python first
find_package(Python COMPONENTS Interpreter Development REQUIRED)

# Try to find pybind11 via CONFIG, fall back to pip-installed version
find_package(pybind11 CONFIG QUIET)
if(NOT pybind11_FOUND)
    # Use pip-installed pybind11
    execute_process(
        COMMAND ${Python_EXECUTABLE} -m pybind11 --cmakedir
        OUTPUT_VARIABLE pybind11_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(pybind11_DIR)
        find_package(pybind11 CONFIG REQUIRED PATHS ${pybind11_DIR} NO_DEFAULT_PATH)
    else()
        message(FATAL_ERROR "pybind11 not found. Please install via pip: pip install pybind11")
    endif()
endif()

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
