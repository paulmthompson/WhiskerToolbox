find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

# --- PythonBridge: embedded interpreter + engine + type bindings ---
# All bindings use PYBIND11_EMBEDDED_MODULE (compiled into the app, no .so).
add_library(PythonBridge STATIC
    # Engine (Phase 1)
    PythonEngine.hpp
    PythonEngine.cpp
    PythonResult.hpp
    OutputRedirector.hpp
    # Bridge (Phase 3)
    PythonBridge.hpp
    PythonBridge.cpp
    # Module entry point + force-linkage helper
    bind_module.hpp
    bindings.cpp
    # Utility headers
    numpy_utils.hpp
    # Per-type binding translation units (Phase 2)
    bind_geometry.cpp
    bind_timeframe.cpp
    bind_entity.cpp
    bind_analog.cpp
    bind_digital_event.cpp
    bind_digital_interval.cpp
    bind_line.cpp
    bind_mask.cpp
    bind_point.cpp
    bind_tensor.cpp
    bind_media.cpp
    bind_datamanager.cpp
)

target_link_libraries(PythonBridge
    PUBLIC  pybind11::embed Python3::Python
    PUBLIC  DataManager                     # all data types (PUBLIC transitive)
    PUBLIC  WhiskerToolbox::TensorData      # TensorData is PRIVATE in DataManager
)

target_include_directories(PythonBridge PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Suppress warnings from pybind11 / Python headers that we cannot fix
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(PythonBridge PRIVATE ${CLANG_OPTIONS})
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(PythonBridge PRIVATE ${GCC_WARNINGS})
endif()
if (MSVC)
    target_compile_options(PythonBridge PRIVATE ${MSVC_WARNINGS})
endif()

add_library(WhiskerToolbox::PythonBridge ALIAS PythonBridge)

