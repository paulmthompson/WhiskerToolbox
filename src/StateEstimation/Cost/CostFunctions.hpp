#include "Common.hpp"

#include "Kalman/KalmanMatrixBuilder.hpp"

namespace StateEstimation {

/**
 * @brief Cost function signature: computes cost between predicted state and observation.
 * 
 * @param predicted_state The predicted state from the filter
 * @param observation The observation feature vector
 * @param num_gap_frames Number of frames in the gap (for gap-dependent costs)
 * @return Cost value (non-negative)
 */
using CostFunction = std::function<double(FilterState const &, Eigen::VectorXd const &, int)>;

/**
 * @brief Factory function to create a Mahalanobis distance-based cost function.
 * 
 * This is the default cost function that computes the Mahalanobis distance between
 * the predicted measurement (H * predicted_state) and the actual observation.
 * 
 * @param H Measurement matrix (maps state space to measurement space)
 * @param R Measurement noise covariance matrix
 * @return CostFunction that computes Mahalanobis distance
 */
CostFunction createMahalanobisCostFunction(Eigen::MatrixXd const & H,
                                           Eigen::MatrixXd const & R);

/**
 * @brief Factory: dynamics-aware transition cost with velocity and implied-acceleration penalties.
 *
 * c = 0.5 r^T S^{-1} r + 0.5 log det S
 *   + beta * 0.5 (v_impl - v_pred)^T Sigma_v^{-1} (v_impl - v_pred)
 *   + gamma * 0.5 ||a_impl||^2 (simple L2 penalty)
 * where r = z_to - H x_pred, S = H P_pred H^T + R,
 * v_impl = (z_pos - x_pred_pos) / (k * dt), and a_impl = 2 * (z_pos - x_pred_pos) / ((k * dt)^2).
 */
CostFunction createDynamicsAwareCostFunction(
        Eigen::MatrixXd const & H,
        Eigen::MatrixXd const & R,
        KalmanMatrixBuilder::StateIndexMap const & index_map,
        double dt,
        double beta = 1.0,
        double gamma = 0.25,
        double lambda_gap = 0.0);

}// namespace StateEstimation