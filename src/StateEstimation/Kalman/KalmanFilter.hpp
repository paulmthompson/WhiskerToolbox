#ifndef KALMAN_FILTER_HPP
#define KALMAN_FILTER_HPP

#include "Filter/IFilter.hpp"


namespace StateEstimation {

/**
     * @brief A concrete implementation of a standard linear Kalman Filter.
     */
class KalmanFilter final : public IFilter {
public:
    /**
         * @brief Constructs a KalmanFilter.
         *
         * @param F State transition matrix.
         * @param H Measurement matrix.
         * @param Q Process noise covariance.
         * @param R Measurement noise covariance.
         */
    KalmanFilter(Eigen::MatrixXd const & F,
                 Eigen::MatrixXd const & H,
                 Eigen::MatrixXd const & Q,
                 Eigen::MatrixXd const & R);

    void initialize(FilterState const & initial_state) override;

    FilterState predict() override;

    FilterState update(FilterState const & predicted_state, Measurement const & measurement) override;

    FilterState update(FilterState const & predicted_state, Measurement const & measurement, double noise_scale_factor) override;

    std::vector<FilterState> smooth(std::vector<FilterState> const & forward_states) override;

    FilterState getState() const override;

    std::unique_ptr<IFilter> clone() const override;



private:
    // Kalman matrices
    Eigen::MatrixXd StateTransitionMat_;// State transition
    Eigen::MatrixXd MeasurementMat_;// Measurement
    Eigen::MatrixXd ProcessNoiseCovMat_;// Process noise covariance
    Eigen::MatrixXd MeasurementNoiseCovMat_;// Measurement noise covariance

    // Filter state
    Eigen::VectorXd StateEstimateVec_;// State estimate vector
    Eigen::MatrixXd StateCovarianceMat_;// State covariance matrix
};

}// namespace StateEstimation

#endif// KALMAN_FILTER_HPP
