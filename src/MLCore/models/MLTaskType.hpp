#ifndef MLCORE_MLTASKTYPE_HPP
#define MLCORE_MLTASKTYPE_HPP

/**
 * @file MLTaskType.hpp
 * @brief Enumeration of ML task types supported by the model operation interface
 *
 * Used to categorize model operations and filter the model registry by task.
 * The MLModelRegistry can be queried for models supporting a specific task type.
 *
 */

#include <string>

namespace MLCore {

/**
 * @brief Describes the type of ML task a model operation supports
 */
enum class MLTaskType {
    BinaryClassification,    ///< Two-class supervised classification
    MultiClassClassification,///< N-class supervised classification (N ≥ 2)
    Clustering,              ///< Unsupervised clustering (assign to K groups)
    DimensionalityReduction, ///< Unsupervised dimensionality reduction (PCA, t-SNE, etc.)
    // Future:
    // Regression,              ///< Continuous value prediction
    // SequenceLabeling,        ///< HMM / sequence-to-sequence labeling
};

/**
 * @brief Human-readable string for an MLTaskType
 */
[[nodiscard]] inline std::string toString(MLTaskType task) {
    switch (task) {
        case MLTaskType::BinaryClassification:
            return "Binary Classification";
        case MLTaskType::MultiClassClassification:
            return "Multi-Class Classification";
        case MLTaskType::Clustering:
            return "Clustering";
        case MLTaskType::DimensionalityReduction:
            return "Dimensionality Reduction";
    }
    return "Unknown";
}

}// namespace MLCore

#endif// MLCORE_MLTASKTYPE_HPP
