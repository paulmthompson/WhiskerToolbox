#include "HungarianAssigner.hpp"

#include "Assignment/hungarian.hpp"// Correctly include your Munkres implementation

#include <cmath>
#include <vector>

namespace StateEstimation {

namespace {
// Helper function to calculate the squared Mahalanobis distance.
double calculateMahalanobisDistanceSq(Eigen::VectorXd const & observation,
                                      Eigen::VectorXd const & predicted_mean,
                                      Eigen::MatrixXd const & predicted_covariance,
                                      Eigen::MatrixXd const & H,
                                      Eigen::MatrixXd const & R) {
    Eigen::VectorXd innovation = observation - (H * predicted_mean);
    Eigen::MatrixXd innovation_covariance = H * predicted_covariance * H.transpose() + R;
    return innovation.transpose() * innovation_covariance.inverse() * innovation;
}
}// namespace

HungarianAssigner::HungarianAssigner(double max_assignment_distance,
                                     Eigen::MatrixXd const & measurement_matrix,
                                     Eigen::MatrixXd const & measurement_noise_covariance,
                                     std::string feature_name)
    : max_assignment_distance_(max_assignment_distance),
      H_(measurement_matrix),
      R_(measurement_noise_covariance),
      feature_name_(std::move(feature_name)) {}

Assignment HungarianAssigner::solve(
        std::vector<Prediction> const & predictions,
        std::vector<Observation> const & observations,
        std::map<EntityId, FeatureCache> const & feature_cache) {

    if (predictions.empty() || observations.empty()) {
        return {};
    }

    // 1. Build the cost matrix with integer costs for the Munkres library.
    int const cost_scaling_factor = 1000;
    int const max_cost = static_cast<int>(max_assignment_distance_ * cost_scaling_factor);
    std::vector<std::vector<int>> cost_matrix(observations.size(), std::vector<int>(predictions.size()));

    for (size_t i = 0; i < observations.size(); ++i) {
        auto cache_it = feature_cache.find(observations[i].entity_id);
        if (cache_it == feature_cache.end()) {
            throw std::runtime_error("Feature cache not found for observation.");
        }

        auto feature_it = cache_it->second.find(feature_name_);
        if (feature_it == cache_it->second.end()) {
            throw std::runtime_error("Required feature '" + feature_name_ + "' not in cache.");
        }

        auto const & observation_vec = std::any_cast<Eigen::VectorXd const &>(feature_it->second);

        for (size_t j = 0; j < predictions.size(); ++j) {
            double dist_sq = calculateMahalanobisDistanceSq(
                    observation_vec,
                    predictions[j].filter_state.state_mean,
                    predictions[j].filter_state.state_covariance,
                    H_,
                    R_);

            double distance = std::sqrt(dist_sq);
            int cost = static_cast<int>(distance * cost_scaling_factor);

            if (cost < max_cost) {
                cost_matrix[i][j] = cost;
            } else {
                // Use a large integer value to represent infinity for the integer-based solver
                cost_matrix[i][j] = std::numeric_limits<int>::max();
            }
        }
    }

    // 2. Solve the assignment problem using the Munkres implementation
    std::vector<std::vector<int>> assignment_matrix;
    Munkres::hungarian_with_assignment(cost_matrix, assignment_matrix);

    // 3. Format the results
    Assignment result;
    for (size_t i = 0; i < assignment_matrix.size(); ++i) {
        for (size_t j = 0; j < assignment_matrix[i].size(); ++j) {
            // A value of 1 in the assignment matrix indicates a match
            if (assignment_matrix[i][j] == 1) {
                // Ensure the assignment is not an "infinite" cost one
                if (cost_matrix[i][j] < std::numeric_limits<int>::max()) {
                    result.observation_to_prediction[i] = j;
                }
                break;// Move to the next observation row
            }
        }
    }

    return result;
}

std::unique_ptr<IAssigner> HungarianAssigner::clone() const {
    return std::make_unique<HungarianAssigner>(*this);
}

}// namespace StateEstimation
