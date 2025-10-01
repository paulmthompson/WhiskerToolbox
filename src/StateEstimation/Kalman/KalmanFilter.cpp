#include "KalmanFilter.hpp"


#include <stdexcept>
#include <iostream>

namespace StateEstimation {

KalmanFilter::KalmanFilter(const Eigen::MatrixXd& F, const Eigen::MatrixXd& H,
                           const Eigen::MatrixXd& Q, const Eigen::MatrixXd& R)
    : F_(F), H_(H), Q_(Q), R_(R) {}

void KalmanFilter::initialize(const FilterState& initial_state) {
    x_ = initial_state.state_mean;
    P_ = initial_state.state_covariance;
}

FilterState KalmanFilter::predict() {
    x_ = F_ * x_;
    P_ = F_ * P_ * F_.transpose() + Q_;
    return {x_, P_};
}

FilterState KalmanFilter::update(const FilterState& predicted_state, const Measurement& measurement) {
    const Eigen::VectorXd& z = measurement.feature_vector;
    
    // Use the predicted state passed in
    Eigen::VectorXd x_pred = predicted_state.state_mean;
    Eigen::MatrixXd P_pred = predicted_state.state_covariance;

    Eigen::VectorXd y = z - H_ * x_pred; // Innovation or residual
    Eigen::MatrixXd S = H_ * P_pred * H_.transpose() + R_; // Innovation covariance
    Eigen::MatrixXd K = P_pred * H_.transpose() * S.inverse(); // Kalman gain

    x_ = x_pred + K * y;
    P_ = (Eigen::MatrixXd::Identity(x_.size(), x_.size()) - K * H_) * P_pred;

    return {x_, P_};
}

std::vector<FilterState> KalmanFilter::smooth(const std::vector<FilterState>& forward_states) {
    if (forward_states.empty()) {
        return {};
    }

    std::vector<FilterState> smoothed_states = forward_states;
    FilterState& last_smoothed = smoothed_states.back();
    
    // The backward pass
    for (int k = static_cast<int>(forward_states.size()) - 2; k >= 0; --k) {
        const FilterState& prev_forward = forward_states[k];

        // Prediction for the next state based on the previous forward state
        Eigen::VectorXd x_pred_next = F_ * prev_forward.state_mean;
        Eigen::MatrixXd P_pred_next = F_ * prev_forward.state_covariance * F_.transpose() + Q_;

        // Smoother gain
        Eigen::MatrixXd Ck = prev_forward.state_covariance * F_.transpose() * P_pred_next.inverse();

        // Update the state and covariance
        smoothed_states[k].state_mean = prev_forward.state_mean + Ck * (last_smoothed.state_mean - x_pred_next);
        smoothed_states[k].state_covariance = prev_forward.state_covariance + Ck * (last_smoothed.state_covariance - P_pred_next) * Ck.transpose();
        
        last_smoothed = smoothed_states[k];
    }

    return smoothed_states;
}

FilterState KalmanFilter::getState() const {
    return {x_, P_};
}

std::unique_ptr<IFilter> KalmanFilter::clone() const {
    return std::make_unique<KalmanFilter>(*this);
}

} // namespace StateEstimation

