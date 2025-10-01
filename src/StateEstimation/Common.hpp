#ifndef STATE_ESTIMATION_COMMON_HPP
#define STATE_ESTIMATION_COMMON_HPP

#include <Eigen/Dense>

#include <any>
#include <map>
#include <vector>

namespace StateEstimation {

// --- Basic Type Definitions ---
using EntityID = uint64_t;
using GroupId = uint64_t;
using FrameIndex = int;

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
    EntityID entity_id;
};

/// @brief The result of the assignment process.
struct Assignment {
    // Maps Observation index to Prediction index.
    std::map<int, int> observation_to_prediction;
};

}// namespace StateEstimation

#endif// STATE_ESTIMATION_COMMON_HPP
