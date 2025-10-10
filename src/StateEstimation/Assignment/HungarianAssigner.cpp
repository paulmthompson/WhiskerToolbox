#include "HungarianAssigner.hpp"

#include "Assignment/hungarian.hpp"// Correctly include your Munkres implementation

#include <cmath>
#include <vector>

namespace StateEstimation {

namespace {
// Helper function to calculate the squared Mahalanobis distance.
// Uses robust matrix solving to handle ill-conditioned covariance matrices
// that can arise from cross-feature correlations.
double calculateMahalanobisDistanceSq(Eigen::VectorXd const & observation,
                                      Eigen::VectorXd const & predicted_mean,
                                      Eigen::MatrixXd const & predicted_covariance,
                                      Eigen::MatrixXd const & H,
                                      Eigen::MatrixXd const & R) {
    // Dimension guard: ensure consistency before multiplying
    if (H.cols() != predicted_mean.size() ||
        predicted_covariance.rows() != predicted_mean.size() ||
        predicted_covariance.cols() != predicted_mean.size() ||
        observation.size() != H.rows()) {
        return 1e10;  // Large but finite cost to avoid assertion and discourage assignment
    }
    Eigen::VectorXd innovation = observation - (H * predicted_mean);
    Eigen::MatrixXd innovation_covariance = H * predicted_covariance * H.transpose() + R;
    
    // Use LLT (Cholesky) decomposition for positive definite matrices
    // This is more numerically stable than direct matrix inversion
    Eigen::LLT<Eigen::MatrixXd> llt(innovation_covariance);
    
    if (llt.info() == Eigen::Success) {
        // Solve: innovation_covariance^-1 * innovation
        Eigen::VectorXd solved = llt.solve(innovation);
        double dist_sq = innovation.transpose() * solved;
        
        // Sanity check: distance should be non-negative
        if (std::isfinite(dist_sq) && dist_sq >= 0.0) {
            return dist_sq;
        }
    }
    
    // Fallback: use pseudo-inverse for ill-conditioned/singular matrices
    // This can happen with strong cross-feature correlations
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(innovation_covariance, 
                                          Eigen::ComputeThinU | Eigen::ComputeThinV);
    
    // Filter out near-zero singular values to handle near-singular matrices
    double tolerance = 1e-10 * svd.singularValues()(0);
    Eigen::VectorXd inv_singular_values = svd.singularValues();
    for (int i = 0; i < inv_singular_values.size(); ++i) {
        if (inv_singular_values(i) > tolerance) {
            inv_singular_values(i) = 1.0 / inv_singular_values(i);
        } else {
            inv_singular_values(i) = 0.0;
        }
    }
    
    // Compute pseudo-inverse: V * Σ^-1 * U^T
    Eigen::MatrixXd pseudo_inv = svd.matrixV() * 
                                 inv_singular_values.asDiagonal() * 
                                 svd.matrixU().transpose();
    
    double dist_sq = innovation.transpose() * pseudo_inv * innovation;
    
    // Return a large but finite distance if still invalid
    if (!std::isfinite(dist_sq) || dist_sq < 0.0) {
        return 1e10;
    }
    
    return dist_sq;
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
    // Track per-observation best candidate for fallback when solver yields no assignment (single prediction case)
    std::vector<int> best_col_for_row(observations.size(), -1);
    std::vector<int> best_cost_for_row(observations.size(), std::numeric_limits<int>::max());

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
            // Keep original costs to allow solver; cap at INT_MAX - 1 to avoid overflow
            if (cost >= std::numeric_limits<int>::max()) {
                cost = std::numeric_limits<int>::max() - 1;
            }
            cost_matrix[i][j] = cost;
            if (cost < best_cost_for_row[i]) {
                best_cost_for_row[i] = cost;
                best_col_for_row[i] = static_cast<int>(j);
            }
        }
    }

    // 2. Solve the assignment problem using the Munkres implementation
    std::vector<std::vector<int>> assignment_matrix;
    Munkres::hungarian_with_assignment(cost_matrix, assignment_matrix);

    // 3. Format the results
    Assignment result;
    result.cost_threshold = max_assignment_distance_;
    
    for (size_t i = 0; i < assignment_matrix.size(); ++i) {
        for (size_t j = 0; j < assignment_matrix[i].size(); ++j) {
            // A value of 1 in the assignment matrix indicates a match
            if (assignment_matrix[i][j] == 1) {
                // Ensure the assignment is not an "infinite" cost one
                if (cost_matrix[i][j] < std::numeric_limits<int>::max()) {
                    result.observation_to_prediction[i] = j;
                    // Store the actual cost (convert back from scaled integer)
                    double actual_cost = static_cast<double>(cost_matrix[i][j]) / cost_scaling_factor;
                    result.assignment_costs[i] = actual_cost;
                }
                break;// Move to the next observation row
            }
        }
    }

    // Fallback for single-prediction case: if solver produced no assignment, pick the best observation greedily
    if (result.observation_to_prediction.empty() && predictions.size() == 1 && !observations.empty()) {
        // Select the observation row with the minimal cost
        int best_row = -1;
        int best_cost = std::numeric_limits<int>::max();
        for (size_t i = 0; i < best_cost_for_row.size(); ++i) {
            if (best_cost_for_row[i] < best_cost && best_col_for_row[i] != -1) {
                best_cost = best_cost_for_row[i];
                best_row = static_cast<int>(i);
            }
        }
        if (best_row != -1) {
            result.observation_to_prediction[best_row] = 0;
            result.assignment_costs[best_row] = static_cast<double>(best_cost) / cost_scaling_factor;
        }
    }

    return result;
}

std::unique_ptr<IAssigner> HungarianAssigner::clone() const {
    return std::make_unique<HungarianAssigner>(*this);
}

}// namespace StateEstimation
