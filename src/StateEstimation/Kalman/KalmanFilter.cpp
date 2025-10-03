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
      P_(Eigen::MatrixXd::Identity(F.rows(), F.rows())),
      F_inv_(Eigen::MatrixXd::Zero(F.rows(), F.cols())),
      Q_backward_(Q.rows(), Q.cols()) {
    if (F_.rows() == F_.cols()) {
        // Compute inverse if well-conditioned; allow Eigen to throw if singular
        F_inv_ = F_.inverse();
        // Backward process noise: Q_b = F_inv * Q * F_inv^T (approximate mapping)
        Q_backward_ = F_inv_ * Q_ * F_inv_.transpose();
        // Ensure symmetry
        Q_backward_ = 0.5 * (Q_backward_ + Q_backward_.transpose());
    }
}

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

// Add the implementation for our new method
std::unique_ptr<IFilter> KalmanFilter::createBackwardFilter() const {
    if (F_inv_.rows() == 0 || F_inv_.cols() == 0) {
        // If the matrix isn't invertible, we can't create a backward filter.
        // Return nullptr or throw an exception.
        return nullptr;
    }
    // Create a new KalmanFilter, but feed it the INVERSE matrices.
    // Its "forward" is our "backward".
    auto backward_filter = std::make_unique<KalmanFilter>(F_inv_, H_, Q_backward_, R_);
    return backward_filter;
}

/*
std::optional<FilterState> KalmanFilter::predictPrevious(FilterState const & current_state) {
    if (F_inv_.rows() == 0 || F_inv_.cols() == 0) {
        return std::nullopt;
    }
    Eigen::VectorXd x_prev = F_inv_ * current_state.state_mean;
    // Covariance backward propagation: P_prev = F_inv * P * F_inv^T + Q_backward
    Eigen::MatrixXd P_prev = F_inv_ * current_state.state_covariance * F_inv_.transpose() + Q_backward_;
    // Force symmetry
    P_prev = 0.5 * (P_prev + P_prev.transpose());
    // Clamp negative eigenvalues to small positive to maintain PSD
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(P_prev);
    Eigen::VectorXd eig = es.eigenvalues();
    Eigen::MatrixXd V = es.eigenvectors();
    double const eps = 1e-9;
    for (int i = 0; i < eig.size(); ++i) {
        if (eig[i] < eps) eig[i] = eps;
    }
    Eigen::MatrixXd P_prev_psd = V * eig.asDiagonal() * V.transpose();
    return FilterState{.state_mean = x_prev, .state_covariance = P_prev_psd};
}
*/

}// namespace StateEstimation
