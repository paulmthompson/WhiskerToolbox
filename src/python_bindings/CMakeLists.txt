find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# ---------------------------------------------------------------------------
# Locate pybind11: Handle both vcpkg (CMake) and uv (Python pip)
# ---------------------------------------------------------------------------
# 1. Try standard CMake discovery first (this natively catches the vcpkg case)
find_package(pybind11 CONFIG QUIET)

if(NOT pybind11_FOUND)
    message(STATUS "[PythonBridge] pybind11 not found in standard paths, checking Python env...")
    
    # 2. Ask the discovered Python interpreter if pybind11 is installed via pip/uv
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -c "import pybind11; print(pybind11.get_cmake_dir())"
        RESULT_VARIABLE _pybind_res
        OUTPUT_VARIABLE pybind11_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET # Suppress the noisy python traceback if it fails
    )

    if(_pybind_res EQUAL 0)
        message(STATUS "[PythonBridge] Found pybind11 CMake dir via Python: ${pybind11_DIR}")
    else()
        message(FATAL_ERROR "[PythonBridge] Could not find pybind11 via CMake or Python environment.")
    endif()

    # Now that we've set pybind11_DIR, require it so the targets are imported
    find_package(pybind11 CONFIG REQUIRED)
else()
    message(STATUS "[PythonBridge] Found pybind11 via standard CMake/vcpkg discovery.")
endif()

# ---------------------------------------------------------------------------
# Locate the Python standard library dynamically
# ---------------------------------------------------------------------------
# Ask Python directly where its prefix and standard library are located
execute_process(
    COMMAND "${Python3_EXECUTABLE}" -c "import sys; print(sys.prefix)"
    OUTPUT_VARIABLE _python_prefix
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
)

execute_process(
    COMMAND "${Python3_EXECUTABLE}" -c "import sysconfig; print(sysconfig.get_path('stdlib'))"
    OUTPUT_VARIABLE _python_stdlib
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
)

# Define where the stdlib should go relative to the executable if bundled
if (WIN32)
    set(_python_stdlib_dest "Lib")          # relative to exe dir
else()
    set(_python_stdlib_dest "lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}")
endif()

message(STATUS "[PythonBridge] Python prefix:  ${_python_prefix}")
message(STATUS "[PythonBridge] Python stdlib:  ${_python_stdlib}")
message(STATUS "[PythonBridge] Python version: ${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}")

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

# ---------------------------------------------------------------------------
# Tell PythonEngine where the stdlib lives relative to the executable.
# At runtime the engine resolves this relative to the exe directory.
# ---------------------------------------------------------------------------
target_compile_definitions(PythonBridge PRIVATE
    WT_PYTHON_VERSION_MAJOR=${Python3_VERSION_MAJOR}
    WT_PYTHON_VERSION_MINOR=${Python3_VERSION_MINOR}
)

# On Windows the stdlib dir name relative to exe is "Lib";
# on Linux it is "lib/pythonX.Y".  The engine uses these at runtime.
if (WIN32)
    target_compile_definitions(PythonBridge PRIVATE
        WT_PYTHON_STDLIB_REL_DIR="Lib"
    )
else()
    target_compile_definitions(PythonBridge PRIVATE
        WT_PYTHON_STDLIB_REL_DIR="lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}"
    )
endif()