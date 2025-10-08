#include "MinCostFlowTracker.hpp"

namespace StateEstimation {


CostFunction createMahalanobisCostFunction(Eigen::MatrixXd const & H,
    Eigen::MatrixXd const & R) {
return [H, R](FilterState const & predicted_state,
Eigen::VectorXd const & observation,
int /* num_gap_frames */) -> double {
Eigen::VectorXd innovation = observation - (H * predicted_state.state_mean);
Eigen::MatrixXd innovation_covariance = H * predicted_state.state_covariance * H.transpose() + R;
double dist_sq = innovation.transpose() * innovation_covariance.inverse() * innovation;
return std::sqrt(dist_sq);
};
}


} // namespace StateEstimation