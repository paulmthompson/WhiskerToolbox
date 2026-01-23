# ML_Widget Static Library
# This library is isolated to avoid OpenMP conflicts with MLPack on Windows

find_package(nlohmann_json CONFIG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)
find_package(LAPACK REQUIRED)
find_package(OpenBLAS REQUIRED)
find_package(Armadillo REQUIRED)

# Find MLPack (header-only library)
find_path(MLPACK_INCLUDE_DIRS "mlpack.hpp")

qt_standard_project_setup()

set(ML_WIDGET_SOURCES
    ML_Widget.hpp
    ML_Widget.cpp
    ML_Widget.ui
    MLWidgetState.hpp
    MLWidgetState.cpp
    MLWidgetStateData.hpp
    MLWidgetRegistration.hpp
    MLWidgetRegistration.cpp
    mlpack_conversion.hpp
    mlpack_conversion.cpp
    MLModelOperation.hpp
    MLModelRegistry.cpp
    MLModelRegistry.hpp
    MLModelParameters.hpp
    MLParameterWidgetBase.hpp
    
    Transformations/TransformationsCommon.hpp
    Transformations/ITransformation.hpp
    Transformations/TransformationBase.cpp
    Transformations/TransformationBase.hpp
    Transformations/IdentityTransform.cpp
    Transformations/IdentityTransform.hpp
    Transformations/SquaredTransform.cpp
    Transformations/SquaredTransform.hpp
    Transformations/LagLeadTransform.cpp
    Transformations/LagLeadTransform.hpp
    
    ClassBalancingWidget/ClassBalancingWidget.cpp
    ClassBalancingWidget/ClassBalancingWidget.hpp
    ClassBalancingWidget/ClassBalancingWidget.ui
    
    FeatureProcessingWidget/FeatureProcessingWidget.cpp
    FeatureProcessingWidget/FeatureProcessingWidget.hpp
    FeatureProcessingWidget/FeatureProcessingWidget.ui
    
    ModelMetricsWidget/ModelMetricsWidget.cpp
    ModelMetricsWidget/ModelMetricsWidget.hpp
    ModelMetricsWidget/ModelMetricsWidget.ui
    
    ML_Random_Forest_Widget/ML_Random_Forest_Widget.hpp
    ML_Random_Forest_Widget/ML_Random_Forest_Widget.cpp
    ML_Random_Forest_Widget/ML_Random_Forest_Widget.ui
    ML_Random_Forest_Widget/RandomForestModelOperation.hpp
    ML_Random_Forest_Widget/RandomForestModelOperation.cpp
    
    ML_Naive_Bayes_Widget/ML_Naive_Bayes_Widget.hpp
    ML_Naive_Bayes_Widget/ML_Naive_Bayes_Widget.cpp
    ML_Naive_Bayes_Widget/ML_Naive_Bayes_Widget.ui
    ML_Naive_Bayes_Widget/NaiveBayesModelOperation.hpp
    ML_Naive_Bayes_Widget/NaiveBayesModelOperation.cpp
)

# Create static library
add_library(ML_Widget STATIC ${ML_WIDGET_SOURCES})

# Set include directories
target_include_directories(ML_Widget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    ${MLPACK_INCLUDE_DIRS}
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Link required libraries
target_link_libraries(ML_Widget PUBLIC
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
)

target_link_libraries(ML_Widget PRIVATE
    nlohmann_json::nlohmann_json
    armadillo
    LAPACK::LAPACK
    OpenBLAS::OpenBLAS
    DataManager
    TensorData
    Feature_Table_Widget
    EditorState
)

# Apply compiler warnings (if defined in parent scope) but remove OpenMP flags
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND DEFINED CLANG_OPTIONS)
    target_compile_options(ML_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND DEFINED GCC_WARNINGS)
    target_compile_options(ML_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC AND DEFINED MSVC_WARNINGS)
    # Create a copy of MSVC_WARNINGS without OpenMP flags
    set(ML_WIDGET_MSVC_WARNINGS ${MSVC_WARNINGS})
    list(FILTER ML_WIDGET_MSVC_WARNINGS EXCLUDE REGEX "/openmp")
    target_compile_options(ML_Widget PRIVATE ${ML_WIDGET_MSVC_WARNINGS})

    # Explicitly disable OpenMP and define macro to prevent MLPack from using it
    target_compile_options(ML_Widget PRIVATE /openmp-)
    target_compile_definitions(ML_Widget PRIVATE MLPACK_HAS_NO_OPENMP)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # On GCC/Clang, undefine _OPENMP to disable OpenMP in MLPack headers
    target_compile_options(ML_Widget PRIVATE -U_OPENMP)
    target_compile_definitions(ML_Widget PRIVATE MLPACK_HAS_NO_OPENMP)
endif()

# Allow compiler/STL version mismatch (for PyTorch compatibility)
target_compile_definitions(ML_Widget PRIVATE _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)

# Set output directories
set_target_properties(ML_Widget PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
