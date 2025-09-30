#ifndef STATE_ESTIMATION_I_FILTER_HPP
#define STATE_ESTIMATION_I_FILTER_HPP

#include <Eigen/Dense>

#include <memory>
#include <vector>

namespace StateEstimation {

/**
 * @brief Represents the state (mean and covariance) of a tracked object.
 * This is the primary data structure passed to and from the filter.
 */
struct FilterState {
    Eigen::VectorXd state_mean;
    Eigen::MatrixXd state_covariance;
};

/**
 * @brief Represents a measurement taken at a specific time, already converted
 * into a feature vector.
 */
struct Measurement {
    Eigen::VectorXd feature_vector;
    // In the future, this could be extended to include measurement uncertainty (R matrix)
    // if it varies per measurement.
};

/**
 * @brief Abstract interface for a state estimation filter.
 *
 * This class defines the contract for all filter implementations, such as
 * KalmanFilter, ExtendedKalmanFilter (EKF), or UnscentedKalmanFilter (UKF).
 * It operates on generic feature vectors, keeping it decoupled from specific
 * data types (like Lines or Points).
 */
class IFilter {
public:
    virtual ~IFilter() = default;

    /**
     * @brief Initializes the filter with an initial state.
     * @param initial_state The starting state (mean and covariance) of the object to be tracked.
     */
    virtual void initialize(FilterState const & initial_state) = 0;

    /**
     * @brief Predicts the next state based on the internal motion model.
     * @return The predicted state (prior estimate).
     */
    virtual FilterState predict() = 0;

    /**
     * @brief Updates (corrects) the filter's state based on a new measurement.
     * @param predicted_state The state predicted by the most recent call to `predict()`.
     * @param measurement The new observation, converted into a feature vector.
     * @return The updated (corrected) state (posterior estimate).
     */
    virtual FilterState update(FilterState const & predicted_state, Measurement const & measurement) = 0;

    /**
     * @brief Performs Rauch-Tung-Striebel (RTS) smoothing on a sequence of states.
     *
     * This method takes a history of states from a forward Kalman filter pass and
     * recursively computes a more accurate, smoothed estimate for the entire sequence.
     *
     * @param forward_states A vector of state estimates from the forward pass, ordered chronologically.
     * This should include both predicted and updated states as required by the
     * RTS algorithm.
     * @return A vector of smoothed FilterState objects, one for each corresponding input state.
     */
    virtual std::vector<FilterState> smooth(std::vector<FilterState> const & forward_states) = 0;

    /**
     * @brief Clones the filter object.
     *
     * This is essential for the Tracker, which will hold a "prototype" filter and clone it
     * for each new group that needs to be tracked.
     *
     * @return A std::unique_ptr to a new instance of the filter with the same configuration.
     */
    virtual std::unique_ptr<IFilter> clone() const = 0;
};

}// namespace StateEstimation

#endif// STATE_ESTIMATION_I_FILTER_HPP
