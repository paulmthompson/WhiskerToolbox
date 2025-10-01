#ifndef STATE_ESTIMATION_I_ASSIGNER_HPP
#define STATE_ESTIMATION_I_ASSIGNER_HPP

#include "Common.hpp"

#include <any>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace StateEstimation {


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
     * @param feature_cache A map where the key is an EntityId from an observation and the value is a
     * cache of its pre-calculated features. The assigner uses this to
     * retrieve the feature vectors it needs for its cost matrix calculation.
     * @return An Assignment object detailing the successful matches.
     */
    virtual Assignment solve(
            std::vector<Prediction> const & predictions,
            std::vector<Observation> const & observations,
            std::map<EntityId, FeatureCache> const & feature_cache) = 0;

    /**
     * @brief Clones the assigner object.
     * @return A std::unique_ptr to a new instance of the assigner with the same configuration.
     */
    virtual std::unique_ptr<IAssigner> clone() const = 0;
};

}// namespace StateEstimation

#endif// I_ASSIGNER_HPP
