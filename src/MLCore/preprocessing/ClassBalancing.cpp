/**
 * @file ClassBalancing.cpp
 * @brief Implementation of class balancing strategies (subsample, oversample)
 */

#include "ClassBalancing.hpp"

#include <algorithm>
#include <map>
#include <numeric>
#include <random>
#include <stdexcept>

namespace MLCore {

// ============================================================================
// Helpers
// ============================================================================

namespace {

/**
 * @brief Create a random engine from config (deterministic or random)
 */
std::mt19937 makeRng(BalancingConfig const & config) {
    if (config.random_seed.has_value()) {
        return std::mt19937{static_cast<std::mt19937::result_type>(*config.random_seed)};
    }
    std::random_device rd;
    return std::mt19937{rd()};
}

/**
 * @brief Gather indices for each class label
 *
 * Returns a map from class label → vector of column indices.
 */
std::map<std::size_t, std::vector<std::size_t>> gatherClassIndices(
        arma::Row<std::size_t> const & labels) {
    std::map<std::size_t, std::vector<std::size_t>> class_indices;
    for (std::size_t i = 0; i < labels.n_elem; ++i) {
        class_indices[labels[i]].push_back(i);
    }
    return class_indices;
}

/**
 * @brief Build BalancedData from selected indices
 */
BalancedData buildFromIndices(
        arma::mat const & features,
        arma::Row<std::size_t> const & labels,
        std::vector<std::size_t> & selected_indices,
        std::mt19937 & rng) {
    // Final shuffle to mix classes
    std::shuffle(selected_indices.begin(), selected_indices.end(), rng);

    BalancedData result;
    result.features.set_size(features.n_rows, selected_indices.size());
    result.labels.set_size(selected_indices.size());
    result.original_indices = selected_indices;

    for (std::size_t i = 0; i < selected_indices.size(); ++i) {
        result.features.col(i) = features.col(selected_indices[i]);
        result.labels[i] = labels[selected_indices[i]];
    }

    // Compute class counts
    std::map<std::size_t, std::size_t> counts;
    for (std::size_t i = 0; i < result.labels.n_elem; ++i) {
        counts[result.labels[i]]++;
    }
    // Store as vector (sparse — only present classes)
    for (auto const & [cls, cnt]: counts) {
        // Ensure vector is large enough
        if (cls >= result.class_counts.size()) {
            result.class_counts.resize(cls + 1, 0);
        }
        result.class_counts[cls] = cnt;
    }

    result.was_balanced = true;
    return result;
}

}// anonymous namespace

// ============================================================================
// balanceClasses — Subsample
// ============================================================================

namespace {

BalancedData subsample(
        arma::mat const & features,
        arma::Row<std::size_t> const & labels,
        BalancingConfig const & config) {
    auto rng = makeRng(config);
    auto class_indices = gatherClassIndices(labels);

    // Find minimum non-zero class count
    std::size_t min_count = std::numeric_limits<std::size_t>::max();
    for (auto const & [cls, indices]: class_indices) {
        if (!indices.empty() && indices.size() < min_count) {
            min_count = indices.size();
        }
    }

    if (min_count == std::numeric_limits<std::size_t>::max()) {
        // All classes empty — shouldn't happen given preconditions
        throw std::invalid_argument("balanceClasses: all classes are empty");
    }

    std::size_t const target_per_class =
            static_cast<std::size_t>(std::round(static_cast<double>(min_count) * config.max_ratio));

    // Subsample each class
    std::vector<std::size_t> selected_indices;
    for (auto & [cls, indices]: class_indices) {
        std::shuffle(indices.begin(), indices.end(), rng);
        std::size_t const take = std::min(indices.size(), target_per_class);
        selected_indices.insert(selected_indices.end(),
                                indices.begin(), indices.begin() + static_cast<std::ptrdiff_t>(take));
    }

    if (selected_indices.empty()) {
        throw std::invalid_argument("balanceClasses: no samples remaining after subsampling");
    }

    return buildFromIndices(features, labels, selected_indices, rng);
}

// ============================================================================
// balanceClasses — Oversample
// ============================================================================

BalancedData oversample(
        arma::mat const & features,
        arma::Row<std::size_t> const & labels,
        BalancingConfig const & config) {
    auto rng = makeRng(config);
    auto class_indices = gatherClassIndices(labels);

    // Find maximum class count
    std::size_t max_count = 0;
    for (auto const & [cls, indices]: class_indices) {
        max_count = std::max(max_count, indices.size());
    }

    if (max_count == 0) {
        throw std::invalid_argument("balanceClasses: all classes are empty");
    }

    // Oversample each class to max_count via random duplication
    std::vector<std::size_t> selected_indices;
    for (auto & [cls, indices]: class_indices) {
        if (indices.empty()) {
            continue;
        }
        // Shuffle so duplicates are random
        std::shuffle(indices.begin(), indices.end(), rng);

        // Add all originals
        selected_indices.insert(selected_indices.end(), indices.begin(), indices.end());

        // Add random duplicates to reach max_count
        std::size_t remaining = max_count - indices.size();
        std::uniform_int_distribution<std::size_t> dist(0, indices.size() - 1);
        for (std::size_t i = 0; i < remaining; ++i) {
            selected_indices.push_back(indices[dist(rng)]);
        }
    }

    return buildFromIndices(features, labels, selected_indices, rng);
}

}// anonymous namespace

// ============================================================================
// Public API
// ============================================================================

BalancedData balanceClasses(
        arma::mat const & features,
        arma::Row<std::size_t> const & labels,
        BalancingConfig const & config) {
    if (features.n_cols != labels.n_elem) {
        throw std::invalid_argument(
                "balanceClasses: features.n_cols (" +
                std::to_string(features.n_cols) +
                ") != labels.n_elem (" +
                std::to_string(labels.n_elem) + ")");
    }

    if (features.n_cols == 0) {
        throw std::invalid_argument("balanceClasses: empty data");
    }

    if (config.max_ratio < 1.0) {
        throw std::invalid_argument(
                "balanceClasses: max_ratio (" +
                std::to_string(config.max_ratio) +
                ") must be >= 1.0");
    }

    // If already balanced, return a copy
    if (isBalanced(labels, config.max_ratio)) {
        BalancedData result;
        result.features = features;
        result.labels = labels;
        result.original_indices.resize(labels.n_elem);
        std::iota(result.original_indices.begin(), result.original_indices.end(), 0);
        result.was_balanced = false;

        std::map<std::size_t, std::size_t> counts;
        for (std::size_t i = 0; i < labels.n_elem; ++i) {
            counts[labels[i]]++;
        }
        for (auto const & [cls, cnt]: counts) {
            if (cls >= result.class_counts.size()) {
                result.class_counts.resize(cls + 1, 0);
            }
            result.class_counts[cls] = cnt;
        }
        return result;
    }

    switch (config.strategy) {
        case BalancingStrategy::Subsample:
            return subsample(features, labels, config);
        case BalancingStrategy::Oversample:
            return oversample(features, labels, config);
    }

    // Unreachable, but satisfy compilers
    return subsample(features, labels, config);
}

std::vector<std::pair<std::size_t, std::size_t>> getClassDistribution(
        arma::Row<std::size_t> const & labels) {
    std::map<std::size_t, std::size_t> counts;
    for (std::size_t i = 0; i < labels.n_elem; ++i) {
        counts[labels[i]]++;
    }
    return {counts.begin(), counts.end()};
}

bool isBalanced(
        arma::Row<std::size_t> const & labels,
        double max_acceptable_ratio) {
    if (labels.n_elem == 0) {
        return true;
    }

    std::map<std::size_t, std::size_t> counts;
    for (std::size_t i = 0; i < labels.n_elem; ++i) {
        counts[labels[i]]++;
    }

    if (counts.size() <= 1) {
        return true;// Single class or empty → balanced by definition
    }

    std::size_t min_count = std::numeric_limits<std::size_t>::max();
    std::size_t max_count = 0;
    for (auto const & [cls, cnt]: counts) {
        if (cnt > 0) {
            min_count = std::min(min_count, cnt);
            max_count = std::max(max_count, cnt);
        }
    }

    if (min_count == 0) {
        return false;// A class with zero samples is always imbalanced
    }

    double const ratio = static_cast<double>(max_count) / static_cast<double>(min_count);
    return ratio <= max_acceptable_ratio;
}

std::string toString(BalancingStrategy strategy) {
    switch (strategy) {
        case BalancingStrategy::Subsample:
            return "Subsample";
        case BalancingStrategy::Oversample:
            return "Oversample";
    }
    return "Unknown";
}

}// namespace MLCore