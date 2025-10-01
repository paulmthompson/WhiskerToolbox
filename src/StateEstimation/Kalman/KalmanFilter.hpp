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
        KalmanFilter(const Eigen::MatrixXd& F,
                     const Eigen::MatrixXd& H,
                     const Eigen::MatrixXd& Q,
                     const Eigen::MatrixXd& R);
    
        void initialize(const FilterState& initial_state) override;
    
        FilterState predict() override;
    
        FilterState update(const FilterState& predicted_state, const Measurement& measurement) override;
    
        std::vector<FilterState> smooth(const std::vector<FilterState>& forward_states) override;
        
        FilterState getState() const override;
    
        std::unique_ptr<IFilter> clone() const override;
    
    
    private:
        // Kalman matrices
        Eigen::MatrixXd F_; // State transition
        Eigen::MatrixXd H_; // Measurement
        Eigen::MatrixXd Q_; // Process noise covariance
        Eigen::MatrixXd R_; // Measurement noise covariance
    
        // Filter state
        Eigen::VectorXd x_; // State estimate vector
        Eigen::MatrixXd P_; // State covariance matrix
    };
    
    } // namespace StateEstimation

#endif // KALMAN_FILTER_HPP
