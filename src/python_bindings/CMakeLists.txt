find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# Locate the Python standard library so we can deploy it next to the
# executable.  The embedded interpreter needs encodings, io, os, etc.
# ---------------------------------------------------------------------------
# Derive the prefix from the Python3 executable found by CMake.
# vcpkg places it under vcpkg_installed/<triplet>/tools/python3/python3.X
get_filename_component(_python_tools_dir "${Python3_EXECUTABLE}" DIRECTORY)

if (WIN32)
    # vcpkg Windows layout: the Python stdlib (Lib/) lives next to the
    # Python executable, i.e. at  tools/python3/Lib/ — NOT at the triplet
    # root.  Probe the most likely locations and pick the first that has
    # the 'encodings' package (the module the interpreter needs first).
    set(_python_prefix "")
    set(_python_stdlib "")

    # Candidate 1: Lib/ next to the Python executable
    if (EXISTS "${_python_tools_dir}/Lib/encodings/__init__.py")
        set(_python_prefix "${_python_tools_dir}")
        set(_python_stdlib "${_python_tools_dir}/Lib")
    endif()

    # Candidate 2: Lib/ at the triplet root (two levels up)
    if (NOT _python_stdlib)
        get_filename_component(_candidate "${_python_tools_dir}" DIRECTORY)
        get_filename_component(_candidate "${_candidate}" DIRECTORY)
        if (EXISTS "${_candidate}/Lib/encodings/__init__.py")
            set(_python_prefix "${_candidate}")
            set(_python_stdlib "${_candidate}/Lib")
        endif()
    endif()

    # Fallback: use the tools dir and hope for the best
    if (NOT _python_stdlib)
        set(_python_prefix "${_python_tools_dir}")
        set(_python_stdlib "${_python_tools_dir}/Lib")
        message(WARNING "[PythonBridge] Could not verify Python stdlib at ${_python_stdlib}")
    endif()

    set(_python_stdlib_dest "Lib")          # relative to exe dir
else()
    # vcpkg Linux layout: tools/python3/python3.12
    # prefix is two levels up:  vcpkg_installed/<triplet>/
    # stdlib is at <prefix>/lib/python<major>.<minor>/
    get_filename_component(_python_prefix "${_python_tools_dir}" DIRECTORY)
    get_filename_component(_python_prefix "${_python_prefix}" DIRECTORY)
    set(_python_stdlib "${_python_prefix}/lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}")
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

