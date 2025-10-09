#include "MinCostFlowTracker.hpp"

namespace StateEstimation {


CostFunction createMahalanobisCostFunction(Eigen::MatrixXd const & H,
    Eigen::MatrixXd const & R) {
return [H, R](FilterState const & predicted_state,
Eigen::VectorXd const & observation,
int /* num_gap_frames */) -> double {
Eigen::VectorXd innovation = observation - (H * predicted_state.state_mean);
Eigen::MatrixXd innovation_covariance = H * predicted_state.state_covariance * H.transpose() + R;

// Use LLT (Cholesky) decomposition for numerical stability with cross-correlated features
Eigen::LLT<Eigen::MatrixXd> llt(innovation_covariance);

if (llt.info() == Eigen::Success) {
    Eigen::VectorXd solved = llt.solve(innovation);
    double dist_sq = innovation.transpose() * solved;
    
    if (std::isfinite(dist_sq) && dist_sq >= 0.0) {
        return std::sqrt(dist_sq);
    }
}

// Fallback: pseudo-inverse for ill-conditioned matrices
Eigen::JacobiSVD<Eigen::MatrixXd> svd(innovation_covariance, 
                                      Eigen::ComputeThinU | Eigen::ComputeThinV);

double tolerance = 1e-10 * svd.singularValues()(0);
Eigen::VectorXd inv_singular_values = svd.singularValues();
for (int i = 0; i < inv_singular_values.size(); ++i) {
    if (inv_singular_values(i) > tolerance) {
        inv_singular_values(i) = 1.0 / inv_singular_values(i);
    } else {
        inv_singular_values(i) = 0.0;
    }
}

Eigen::MatrixXd pseudo_inv = svd.matrixV() * 
                             inv_singular_values.asDiagonal() * 
                             svd.matrixU().transpose();

double dist_sq = innovation.transpose() * pseudo_inv * innovation;

if (!std::isfinite(dist_sq) || dist_sq < 0.0) {
    return 1e5;  // Large but finite distance
}

return std::sqrt(dist_sq);
};
}


} // namespace StateEstimation