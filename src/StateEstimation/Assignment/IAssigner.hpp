#ifndef STATE_ESTIMATION_I_ASSIGNER_HPP
#define STATE_ESTIMATION_I_ASSIGNER_HPP

#include "Filter/IFilter.hpp"// For FilterState

#include <any>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace StateEstimation {

// Type aliases for core identifiers, assumed to be defined elsewhere in the application.
// These are included here for clarity.
using GroupId = uint64_t;
using EntityID = uint64_t;

/**
 * @brief A cache for storing pre-calculated features of a data object for a single frame.
 *
 * This uses the Flyweight pattern to avoid re-calculating features that might be needed
 * by multiple parts of the tracking algorithm (e.g., both filtering and assignment).
 * The key is a user-defined string (e.g., "centroid", "length").
 */
using FeatureCache = std::map<std::string, std::any>;

/**
 * @brief Represents a predicted state for a tracked group, ready for assignment.
 */
struct Prediction {
    GroupId group_id;
    FilterState filter_state;
};

/**
 * @brief Represents an unassigned observation in the current frame.
 */
struct Observation {
    EntityID entity_id;
};

/**
 * @brief Holds the results of the assignment process.
 */
struct Assignment {
    // Maps the index of an observation in the input vector to the index
    // of a prediction in the input vector.
    // Unassigned observations will not have an entry in this map.
    std::map<int, int> observation_to_prediction;
};

/**
 * @brief Abstract interface for a data association (assignment) algorithm.
 *
 * This class defines the contract for algorithms that solve the assignment problem,
 * such as the Hungarian algorithm or a simple nearest-neighbor search. It uses a
 * feature cache as a mediator to remain decoupled from the feature extraction process.
 */
class IAssigner {
public:
    virtual ~IAssigner() = default;

    /**
     * @brief Solves the assignment problem for a set of predictions and observations.
     *
     * @param predictions A vector of predicted states from the filter for each active track.
     * @param observations A vector of new, unassigned observations from the current frame.
     * @param feature_cache A map where the key is an EntityID from an observation and the value is a
     * cache of its pre-calculated features. The assigner uses this to
     * retrieve the feature vectors it needs for its cost matrix calculation.
     * @return An Assignment object detailing the successful matches.
     */
    virtual Assignment solve(
            std::vector<Prediction> const & predictions,
            std::vector<Observation> const & observations,
            std::map<EntityID, FeatureCache> const & feature_cache) = 0;

    /**
     * @brief Clones the assigner object.
     * @return A std::unique_ptr to a new instance of the assigner with the same configuration.
     */
    virtual std::unique_ptr<IAssigner> clone() const = 0;
};

}// namespace StateEstimation

#endif// I_ASSIGNER_HPP
