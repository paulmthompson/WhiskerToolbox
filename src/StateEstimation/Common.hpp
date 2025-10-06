#ifndef STATE_ESTIMATION_COMMON_HPP
#define STATE_ESTIMATION_COMMON_HPP

#include "Entity/EntityTypes.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <Eigen/Dense>

#include <any>
#include <map>
#include <vector>

namespace StateEstimation {

// Note: EntityId, GroupId, and TimeFrameIndex are now imported from Entity/ and TimeFrame/

// --- Core Data Structures ---

/// @brief Represents the state (mean and covariance) of a tracked object.
struct FilterState {
    Eigen::VectorXd state_mean;
    Eigen::MatrixXd state_covariance;
};

/// @brief Represents a measurement (an extracted feature vector).
struct Measurement {
    Eigen::VectorXd feature_vector;
};

/// @brief Stores memoized feature calculations for a single data object.
using FeatureCache = std::map<std::string, std::any>;

/// @brief Represents a predicted state for a tracked group.
struct Prediction {
    GroupId group_id;
    FilterState filter_state;
};

/// @brief Represents an unassigned observation.
struct Observation {
    EntityId entity_id;
};

/// @brief The result of the assignment process.
struct Assignment {
    // Maps Observation index to Prediction index.
    std::map<int, int> observation_to_prediction;
    
    // Cost information for each assignment (for identity confidence tracking)
    std::map<int, double> assignment_costs;
    
    // Maximum cost threshold used for this assignment
    double cost_threshold = 1.0;
};

}// namespace StateEstimation

#endif// STATE_ESTIMATION_COMMON_HPP
