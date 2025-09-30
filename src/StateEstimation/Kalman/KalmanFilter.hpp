#ifndef KALMAN_FILTER_HPP
#define KALMAN_FILTER_HPP

#include "Filter/IFilter.hpp"

#include <stdexcept>

namespace StateEstimation {

/**
 * @brief A concrete implementation of the IFilter interface using a standard linear Kalman filter.
 *
 * This class encapsulates the state, covariance, and system model matrices (F, H, Q, R)
 * required for Kalman filtering. It provides methods for prediction, updating with measurements,
 * and performing RTS smoothing.
 */
class KalmanFilter final : public IFilter {
public:
    /**
     * @brief Constructs a KalmanFilter.
     *
     * @param state_transition_matrix (F) The matrix that describes how the state evolves from one time step to the next.
     * @param measurement_matrix (H) The matrix that maps the state space to the measurement space.
     * @param process_noise_covariance (Q) The covariance of the process noise.
     * @param measurement_noise_covariance (R) The covariance of the measurement noise.
     */
    KalmanFilter(const Eigen::MatrixXd& state_transition_matrix,
                 const Eigen::MatrixXd& measurement_matrix,
                 const Eigen::MatrixXd& process_noise_covariance,
                 const Eigen::MatrixXd& measurement_noise_covariance)
        : F_(state_transition_matrix),
          H_(measurement_matrix),
          Q_(process_noise_covariance),
          R_(measurement_noise_covariance),
          is_initialized_(false) {
        if (F_.rows() != F_.cols() || H_.cols() != F_.rows() || Q_.rows() != F_.rows() || Q_.cols() != F_.rows() || R_.rows() != H_.rows() || R_.cols() != H_.rows()) {
            throw std::invalid_argument("KalmanFilter matrix dimensions are inconsistent.");
        }
        I_ = Eigen::MatrixXd::Identity(F_.rows(), F_.rows());
    }

    void initialize(const FilterState& initial_state) override {
        current_state_ = initial_state;
        is_initialized_ = true;
    }

    FilterState predict() override {
        if (!is_initialized_) {
            throw std::runtime_error("Filter must be initialized before predict.");
        }

        // Predict state: x_hat_priori = F * x_hat_posteriori
        current_state_.state_mean = F_ * current_state_.state_mean;

        // Predict covariance: P_priori = F * P_posteriori * F' + Q
        current_state_.state_covariance = F_ * current_state_.state_covariance * F_.transpose() + Q_;

        return current_state_;
    }

    FilterState update(const FilterState& predicted_state, const Measurement& measurement) override {
        if (!is_initialized_) {
            throw std::runtime_error("Filter must be initialized before update.");
        }

        // Innovation: y = z - H * x_hat_priori
        Eigen::VectorXd y = measurement.feature_vector - (H_ * predicted_state.state_mean);

        // Innovation covariance: S = H * P_priori * H' + R
        Eigen::MatrixXd S = H_ * predicted_state.state_covariance * H_.transpose() + R_;

        // Kalman gain: K = P_priori * H' * S^-1
        Eigen::MatrixXd K = predicted_state.state_covariance * H_.transpose() * S.inverse();

        // Update state: x_hat_posteriori = x_hat_priori + K * y
        current_state_.state_mean = predicted_state.state_mean + (K * y);

        // Update covariance: P_posteriori = (I - K * H) * P_priori
        current_state_.state_covariance = (I_ - K * H_) * predicted_state.state_covariance;

        return current_state_;
    }

    std::vector<FilterState> smooth(const std::vector<FilterState>& forward_states) override {
        if (forward_states.empty()) {
            return {};
        }

        std::vector<FilterState> smoothed_states = forward_states;
        size_t n = forward_states.size();

        // The backward pass starts from the second to last state.
        for (size_t k = n - 2; k < n; --k) { // Loop backwards (k = n-2, n-3, ..., 0)
            const auto& state_k_updated = forward_states[k];

            // Re-predict to get the prior state at k+1, which is needed for the smoother gain.
            // P_{k+1|k} = F * P_{k|k} * F' + Q
            Eigen::MatrixXd P_k_plus_1_predicted = F_ * state_k_updated.state_covariance * F_.transpose() + Q_;

            // Smoother Gain: C_k = P_{k|k} * F' * (P_{k+1|k})^-1
            Eigen::MatrixXd Ck = state_k_updated.state_covariance * F_.transpose() * P_k_plus_1_predicted.inverse();

            // Smoothed state: x_{k|n} = x_{k|k} + C_k * (x_{k+1|n} - x_{k+1|k})
            // Note: x_{k+1|k} is the predicted state at k+1.
            Eigen::VectorXd x_k_plus_1_predicted = F_ * state_k_updated.state_mean;
            smoothed_states[k].state_mean = state_k_updated.state_mean + Ck * (smoothed_states[k + 1].state_mean - x_k_plus_1_predicted);

            // Smoothed covariance: P_{k|n} = P_{k|k} + C_k * (P_{k+1|n} - P_{k+1|k}) * C_k'
            smoothed_states[k].state_covariance = state_k_updated.state_covariance + Ck * (smoothed_states[k + 1].state_covariance - P_k_plus_1_predicted) * Ck.transpose();
        }

        return smoothed_states;
    }

    std::unique_ptr<IFilter> clone() const override {
        return std::make_unique<KalmanFilter>(*this);
    }

private:
    // Core Kalman Filter Matrices
    Eigen::MatrixXd F_; // State Transition Matrix
    Eigen::MatrixXd H_; // Measurement Matrix
    Eigen::MatrixXd Q_; // Process Noise Covariance
    Eigen::MatrixXd R_; // Measurement Noise Covariance
    Eigen::MatrixXd I_; // Identity Matrix

    // Internal State
    FilterState current_state_;
    bool is_initialized_;
};

} // namespace StateEstimation

#endif // KALMAN_FILTER_HPP
