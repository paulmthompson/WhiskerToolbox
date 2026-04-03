
find_package(LAPACK REQUIRED)
find_package(OpenBLAS REQUIRED)
find_package(Armadillo REQUIRED)

# Find MLPack (header-only library)
find_path(MLPACK_INCLUDE_DIRS "mlpack.hpp")

set(MLCore_SOURCES

    entity_utils/TimeEntityUtils.cpp 
    entity_utils/TimeEntityUtils.hpp

    features/FeatureConverter.cpp
    features/FeatureConverter.hpp
    features/FeatureValidator.cpp
    features/FeatureValidator.hpp
    features/LabelAssembler.cpp
    features/LabelAssembler.hpp

    metrics/ClassificationMetrics.cpp
    metrics/ClassificationMetrics.hpp

    models/MLModelOperation.hpp
    models/MLModelParameters.hpp
    models/MLModelRegistry.cpp
    models/MLModelRegistry.hpp
    models/MLTaskType.hpp

    models/MLDimReductionOperation.hpp
    models/MLSupervisedDimReductionOperation.hpp

    models/supervised/LogisticRegressionOperation.cpp
    models/supervised/LogisticRegressionOperation.hpp
    models/supervised/LogitProjectionOperation.cpp
    models/supervised/LogitProjectionOperation.hpp
    models/supervised/SupervisedPCAOperation.cpp
    models/supervised/SupervisedPCAOperation.hpp
    models/supervised/SupervisedRobustPCAOperation.cpp
    models/supervised/SupervisedRobustPCAOperation.hpp
    models/supervised/SoftmaxRegressionOperation.cpp
    models/supervised/SoftmaxRegressionOperation.hpp
    models/supervised/NaiveBayesOperation.cpp
    models/supervised/NaiveBayesOperation.hpp
    models/supervised/HiddenMarkovModelOperation.cpp
    models/supervised/HiddenMarkovModelOperation.hpp
    models/supervised/RandomForestOperation.cpp
    models/supervised/RandomForestOperation.hpp

    models/unsupervised/DBSCANOperation.cpp
    models/unsupervised/DBSCANOperation.hpp
    models/unsupervised/GaussianMixtureOperation.cpp
    models/unsupervised/GaussianMixtureOperation.hpp
    models/unsupervised/KMeansOperation.cpp
    models/unsupervised/KMeansOperation.hpp
    models/unsupervised/PCAOperation.cpp
    models/unsupervised/PCAOperation.hpp
    models/unsupervised/RobustPCAOperation.cpp
    models/unsupervised/RobustPCAOperation.hpp
    models/unsupervised/rosl.hpp
    models/unsupervised/TSNEOperation.cpp
    models/unsupervised/TSNEOperation.hpp

    output/PredictionWriter.cpp
    output/PredictionWriter.hpp

    pipelines/BoundingSpan.cpp
    pipelines/BoundingSpan.hpp

    pipelines/ClassificationPipeline.cpp
    pipelines/ClassificationPipeline.hpp

    pipelines/ClusteringPipeline.cpp
    pipelines/ClusteringPipeline.hpp

    pipelines/DimReductionPipeline.cpp
    pipelines/DimReductionPipeline.hpp

    pipelines/SupervisedDimReductionPipeline.cpp
    pipelines/SupervisedDimReductionPipeline.hpp

    pipelines/SequenceAssembler.cpp
    pipelines/SequenceAssembler.hpp

    preprocessing/ClassBalancing.cpp
    preprocessing/ClassBalancing.hpp

)

add_library(MLCore STATIC ${MLCore_SOURCES})

# Set include directories
target_include_directories(MLCore PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    ${MLPACK_INCLUDE_DIRS}
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(MLCore PRIVATE
   # nlohmann_json::nlohmann_json
    armadillo
    LAPACK::LAPACK
    OpenBLAS::OpenBLAS
    DataManager
    TensorData
    tapkee::tapkee
    #Feature_Table_Widget
)

# Apply compiler warnings (if defined in parent scope) but remove OpenMP flags
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND DEFINED CLANG_OPTIONS)
    target_compile_options(MLCore PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND DEFINED GCC_WARNINGS)
    target_compile_options(MLCore PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC AND DEFINED MSVC_WARNINGS)
    # Create a copy of MSVC_WARNINGS without OpenMP flags
    set(MLCore_MSVC_WARNINGS ${MSVC_WARNINGS})
    list(FILTER MLCore_MSVC_WARNINGS EXCLUDE REGEX "/openmp")
    target_compile_options(MLCore PRIVATE ${MLCore_MSVC_WARNINGS})

    # Explicitly disable OpenMP and define macro to prevent MLPack from using it
    target_compile_options(MLCore PRIVATE /openmp-)
    target_compile_definitions(MLCore PRIVATE MLPACK_HAS_NO_OPENMP)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # On GCC/Clang, undefine _OPENMP to disable OpenMP in MLPack headers
    target_compile_options(MLCore PRIVATE -U_OPENMP)
    target_compile_definitions(MLCore PRIVATE MLPACK_HAS_NO_OPENMP)
endif()

# Allow compiler/STL version mismatch (for PyTorch compatibility)
target_compile_definitions(MLCore PRIVATE _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)

# Set output directories
set_target_properties(MLCore PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

enable_whiskertoolbox_pch(MLCore)
