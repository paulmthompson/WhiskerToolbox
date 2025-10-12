#include "KalmanFilter.hpp"


#include <iostream>
#include <stdexcept>

namespace StateEstimation {

KalmanFilter::KalmanFilter(Eigen::MatrixXd const & F, Eigen::MatrixXd const & H,
                           Eigen::MatrixXd const & Q, Eigen::MatrixXd const & R)
    : StateTransitionMat_(F),
      MeasurementMat_(H),
      ProcessNoiseCovMat_(Q),
      MeasurementNoiseCovMat_(R),
      StateEstimateVec_(Eigen::VectorXd::Zero(F.rows())),
      StateCovarianceMat_(Eigen::MatrixXd::Identity(F.rows(), F.rows())) {}

void KalmanFilter::initialize(FilterState const & initial_state) {
    StateEstimateVec_ = initial_state.state_mean;
    StateCovarianceMat_ = initial_state.state_covariance;
}

FilterState KalmanFilter::predict() {
    StateEstimateVec_ = StateTransitionMat_ * StateEstimateVec_;
    StateCovarianceMat_ = StateTransitionMat_ * StateCovarianceMat_ * StateTransitionMat_.transpose() + ProcessNoiseCovMat_;
    
    // Force symmetry to counteract numerical errors
    StateCovarianceMat_ = 0.5 * (StateCovarianceMat_ + StateCovarianceMat_.transpose());
    
    // Ensure positive semi-definite by clamping negative eigenvalues
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(StateCovarianceMat_);
    Eigen::VectorXd eig = es.eigenvalues();
    Eigen::MatrixXd V = es.eigenvectors();
    double const eps = 1e-9;
    bool has_negative = false;
    for (int i = 0; i < eig.size(); ++i) {
        if (eig[i] < eps) {
            eig[i] = eps;
            has_negative = true;
        }
    }
    if (has_negative) {
        StateCovarianceMat_ = V * eig.asDiagonal() * V.transpose();
    }
    
    return FilterState{.state_mean = StateEstimateVec_, .state_covariance = StateCovarianceMat_};
}

FilterState KalmanFilter::update(FilterState const & predicted_state, Measurement const & measurement) {
    return update(predicted_state, measurement, 1.0);
}

FilterState KalmanFilter::update(FilterState const & predicted_state, Measurement const & measurement, double noise_scale_factor) {
    Eigen::VectorXd const & z = measurement.feature_vector;

    // Use the predicted state passed in
    Eigen::VectorXd const x_pred = predicted_state.state_mean;
    Eigen::MatrixXd const P_pred = predicted_state.state_covariance;

    // Dimension guard: ensure H_ matches state and z matches H_ rows
    if (MeasurementMat_.cols() != x_pred.size() || P_pred.rows() != x_pred.size() || P_pred.cols() != x_pred.size() || z.size() != MeasurementMat_.rows()) {
        // Keep internal state unchanged; return current state and log warning
        return FilterState{.state_mean = StateEstimateVec_, .state_covariance = StateCovarianceMat_};
    }

    // Scale the measurement noise matrix R by the noise scale factor
    Eigen::MatrixXd const R_scaled = noise_scale_factor * MeasurementNoiseCovMat_;

    Eigen::VectorXd const y = z - MeasurementMat_ * x_pred;                      // Innovation or residual
    Eigen::MatrixXd S = MeasurementMat_ * P_pred * MeasurementMat_.transpose() + R_scaled;    // Innovation covariance
    Eigen::MatrixXd K = P_pred * MeasurementMat_.transpose() * S.inverse();// Kalman gain

    StateEstimateVec_ = x_pred + K * y;
    
    // Use Joseph form for covariance update (guarantees PSD)
    Eigen::MatrixXd I = Eigen::MatrixXd::Identity(StateEstimateVec_.size(), StateEstimateVec_.size());
    Eigen::MatrixXd A = I - K * MeasurementMat_;
    StateCovarianceMat_ = A * P_pred * A.transpose() + K * R_scaled * K.transpose();
    
    // Force symmetry
    StateCovarianceMat_ = 0.5 * (StateCovarianceMat_ + StateCovarianceMat_.transpose());

    return FilterState{.state_mean = StateEstimateVec_, .state_covariance = StateCovarianceMat_};
}

std::vector<FilterState> KalmanFilter::smooth(std::vector<FilterState> const & forward_states) {
    if (forward_states.empty()) {
        return {};
    }

    std::vector<FilterState> smoothed_states = forward_states;

    // The backward pass (Rauch-Tung-Striebel smoother)
    // Propagate information backward from k+1 to k
    for (int k = static_cast<int>(forward_states.size()) - 2; k >= 0; --k) {
        FilterState const & fwd_k = forward_states[k];
        FilterState const & smoothed_k_plus_1 = smoothed_states[k + 1];

        // Prediction for k+1 based on forward estimate at k
        Eigen::VectorXd x_pred_k_plus_1 = StateTransitionMat_ * fwd_k.state_mean;
        Eigen::MatrixXd P_pred_k_plus_1 = StateTransitionMat_ * fwd_k.state_covariance * StateTransitionMat_.transpose() + ProcessNoiseCovMat_;
        
        // Force symmetry on predicted covariance
        P_pred_k_plus_1 = 0.5 * (P_pred_k_plus_1 + P_pred_k_plus_1.transpose());

        // Smoother gain: C_k = P_k * F^T * P_pred^{-1}
        Eigen::MatrixXd Ck = fwd_k.state_covariance * StateTransitionMat_.transpose() * P_pred_k_plus_1.inverse();

        // Smoothed estimate at k incorporating information from future (k+1)
        smoothed_states[k].state_mean = fwd_k.state_mean + Ck * (smoothed_k_plus_1.state_mean - x_pred_k_plus_1);
        smoothed_states[k].state_covariance = fwd_k.state_covariance + Ck * (smoothed_k_plus_1.state_covariance - P_pred_k_plus_1) * Ck.transpose();
        
        // Force symmetry on smoothed covariance
        smoothed_states[k].state_covariance = 0.5 * (smoothed_states[k].state_covariance + smoothed_states[k].state_covariance.transpose());
    }

    return smoothed_states;
}

FilterState KalmanFilter::getState() const {
    return FilterState{.state_mean = StateEstimateVec_, .state_covariance = StateCovarianceMat_};
}

std::unique_ptr<IFilter> KalmanFilter::clone() const {
    return std::make_unique<KalmanFilter>(*this);
}


}// namespace StateEstimation
