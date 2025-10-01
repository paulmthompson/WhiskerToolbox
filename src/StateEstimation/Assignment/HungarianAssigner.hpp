#ifndef HUNGARIAN_ASSIGNER_HPP
#define HUNGARIAN_ASSIGNER_HPP

#include "Assignment/IAssigner.hpp"
#include "Assignment/hungarian.hpp"

#include <string>

namespace StateEstimation {

/**
 * @brief An implementation of IAssigner that uses the Hungarian algorithm.
 *
 * This class calculates a cost matrix based on the Mahalanobis distance between
 * predicted states and observed measurements. It then uses the Hungarian algorithm
 * to find the optimal assignment with the minimum total cost.
 */
class HungarianAssigner final : public IAssigner {
public:
    /**
     * @brief Constructs a HungarianAssigner.
     *
     * @param max_assignment_distance The maximum allowable Mahalanobis distance for an assignment to be considered valid.
     * @param measurement_matrix (H) The matrix that maps the state space to the measurement space.
     * @param measurement_noise_covariance (R) The covariance of the measurement noise.
     * @param feature_name The name of the feature to extract from the cache for distance calculation (e.g., "position").
     */
    HungarianAssigner(double max_assignment_distance,
                      Eigen::MatrixXd const & measurement_matrix,
                      Eigen::MatrixXd const & measurement_noise_covariance,
                      std::string feature_name = "kalman_features");

    Assignment solve(
            std::vector<Prediction> const & predictions,
            std::vector<Observation> const & observations,
            std::map<EntityId, FeatureCache> const & feature_cache) override;

    std::unique_ptr<IAssigner> clone() const override;

private:
    double max_assignment_distance_;
    Eigen::MatrixXd H_;// Measurement Matrix
    Eigen::MatrixXd R_;// Measurement Noise Covariance
    std::string feature_name_;
};

}// namespace StateEstimation

#endif// HUNGARIAN_ASSIGNER_HPP
