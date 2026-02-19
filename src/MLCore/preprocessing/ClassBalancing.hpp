#ifndef MLCORE_CLASSBALANCING_HPP
#define MLCORE_CLASSBALANCING_HPP

/**
 * @file ClassBalancing.hpp
 * @brief Class balancing utilities for imbalanced training data
 *
 * Provides strategies for addressing class imbalance before training:
 *
 * - **Subsample**: Randomly down-sample majority classes to match the minority
 *   class count (optionally scaled by a ratio). This is the simplest approach
 *   and the one used by the legacy ML_Widget.
 * - **Oversample**: Randomly duplicate minority class samples to match the
 *   majority class count. Simple but can lead to overfitting.
 * - **SMOTE**: Placeholder for Synthetic Minority Oversampling. Not yet
 *   implemented — falls back to Oversample with a warning.
 *
 * All functions operate on mlpack-layout matrices (features × observations)
 * with a corresponding label row vector. The output is a new balanced pair
 * of (features, labels) with observations shuffled.
 *
 * Extracted and generalized from the legacy `balance_training_data_by_subsampling`
 * in mlpack_conversion.cpp.
 *
 * @see ml_library_roadmap.md §3.4.7
 */

#include <armadillo>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace MLCore {

// ============================================================================
// Configuration
// ============================================================================

/**
 * @brief Strategy used for class balancing
 */
enum class BalancingStrategy {
    Subsample,    ///< Down-sample majority classes to minority count × ratio
    Oversample,   ///< Up-sample minority classes to majority count × ratio
    // SMOTE,     ///< Synthetic Minority Oversampling (future)
};

/**
 * @brief Configuration for class balancing
 */
struct BalancingConfig {
    BalancingStrategy strategy = BalancingStrategy::Subsample;

    /**
     * @brief Target ratio relative to the reference class count
     *
     * For Subsample: each class is capped at min_class_count × max_ratio.
     * For Oversample: each class is raised to max_class_count (ratio unused).
     * Must be >= 1.0.
     */
    double max_ratio = 1.0;

    /**
     * @brief Optional random seed for reproducible results
     *
     * When nullopt, std::random_device is used for non-deterministic shuffling.
     */
    std::optional<std::size_t> random_seed;
};

// ============================================================================
// Result type
// ============================================================================

/**
 * @brief Result of a class balancing operation
 */
struct BalancedData {
    arma::mat features;              ///< Balanced feature matrix (features × observations)
    arma::Row<std::size_t> labels;   ///< Balanced label vector

    /**
     * @brief Per-class sample counts after balancing
     *
     * Index is the class label, value is the count.
     */
    std::vector<std::size_t> class_counts;

    /**
     * @brief Original indices into the input data for each balanced observation
     *
     * balanced_data.features.col(i) came from original features.col(original_indices[i]).
     * Useful for traceability.
     */
    std::vector<std::size_t> original_indices;

    /**
     * @brief Whether any balancing was actually performed
     *
     * False if the data was already balanced (all classes equal) or if
     * balancing failed for some reason (in which case features/labels
     * contain a copy of the original data).
     */
    bool was_balanced = false;
};

// ============================================================================
// Balancing functions
// ============================================================================

/**
 * @brief Balance training data according to the given strategy
 *
 * This is the primary entry point. Features are in mlpack layout
 * (features × observations), and labels is a row vector of class indices.
 *
 * @param features Input feature matrix (features × observations)
 * @param labels   Input label vector (one per observation column)
 * @param config   Balancing configuration
 * @return BalancedData with balanced features, labels, and metadata
 * @throws std::invalid_argument if features.n_cols != labels.n_elem,
 *         or if max_ratio < 1.0, or if data is empty
 *
 * @pre features.n_cols == labels.n_elem
 * @pre config.max_ratio >= 1.0
 * @pre features.n_cols > 0
 */
[[nodiscard]] BalancedData balanceClasses(
    arma::mat const & features,
    arma::Row<std::size_t> const & labels,
    BalancingConfig const & config = {});

/**
 * @brief Compute per-class sample counts from a label vector
 *
 * @param labels Label vector
 * @return Map from class label → count (sorted by label)
 */
[[nodiscard]] std::vector<std::pair<std::size_t, std::size_t>> getClassDistribution(
    arma::Row<std::size_t> const & labels);

/**
 * @brief Check whether a dataset is balanced within a given tolerance
 *
 * A dataset is considered balanced if the ratio of the largest class to
 * the smallest (non-zero) class is <= max_acceptable_ratio.
 *
 * @param labels Label vector
 * @param max_acceptable_ratio Maximum allowed ratio (default 1.0 = perfectly balanced)
 * @return true if already balanced within tolerance
 */
[[nodiscard]] bool isBalanced(
    arma::Row<std::size_t> const & labels,
    double max_acceptable_ratio = 1.0);

/**
 * @brief Convert a BalancingStrategy enum to a human-readable string
 */
[[nodiscard]] std::string toString(BalancingStrategy strategy);

} // namespace MLCore

#endif // MLCORE_CLASSBALANCING_HPP