#include "KalmanFilter.hpp"


#include <iostream>
#include <stdexcept>

namespace StateEstimation {

KalmanFilter::KalmanFilter(Eigen::MatrixXd const & F, Eigen::MatrixXd const & H,
                           Eigen::MatrixXd const & Q, Eigen::MatrixXd const & R)
    : F_(F),
      H_(H),
      Q_(Q),
      R_(R),
      x_(Eigen::VectorXd::Zero(F.rows())),
      P_(Eigen::MatrixXd::Identity(F.rows(), F.rows())) {}

void KalmanFilter::initialize(FilterState const & initial_state) {
    x_ = initial_state.state_mean;
    P_ = initial_state.state_covariance;
}

FilterState KalmanFilter::predict() {
    x_ = F_ * x_;
    P_ = F_ * P_ * F_.transpose() + Q_;
    return FilterState{.state_mean = x_, .state_covariance = P_};
}

FilterState KalmanFilter::update(FilterState const & predicted_state, Measurement const & measurement) {
    return update(predicted_state, measurement, 1.0);
}

FilterState KalmanFilter::update(FilterState const & predicted_state, Measurement const & measurement, double noise_scale_factor) {
    Eigen::VectorXd const & z = measurement.feature_vector;

    // Use the predicted state passed in
    Eigen::VectorXd const x_pred = predicted_state.state_mean;
    Eigen::MatrixXd const P_pred = predicted_state.state_covariance;

    // Scale the measurement noise matrix R by the noise scale factor
    Eigen::MatrixXd const R_scaled = noise_scale_factor * R_;

    Eigen::VectorXd const y = z - H_ * x_pred;                      // Innovation or residual
    Eigen::MatrixXd S = H_ * P_pred * H_.transpose() + R_scaled;    // Innovation covariance
    Eigen::MatrixXd K = P_pred * H_.transpose() * S.inverse();// Kalman gain

    x_ = x_pred + K * y;
    P_ = (Eigen::MatrixXd::Identity(x_.size(), x_.size()) - K * H_) * P_pred;

    return FilterState{.state_mean = x_, .state_covariance = P_};
}

std::vector<FilterState> KalmanFilter::smooth(std::vector<FilterState> const & forward_states) {
    if (forward_states.empty()) {
        return {};
    }

    std::vector<FilterState> smoothed_states = forward_states;
    FilterState & last_smoothed = smoothed_states.back();

    // The backward pass
    for (int k = static_cast<int>(forward_states.size()) - 2; k >= 0; --k) {
        FilterState const & prev_forward = forward_states[k];

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
    return FilterState{.state_mean = x_, .state_covariance = P_};
}

std::unique_ptr<IFilter> KalmanFilter::clone() const {
    return std::make_unique<KalmanFilter>(*this);
}

}// namespace StateEstimation
