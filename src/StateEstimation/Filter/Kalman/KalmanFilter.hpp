#ifndef KALMAN_FILTER_HPP
#define KALMAN_FILTER_HPP

#include "Filter/Kalman/KalmanFilterT.hpp"

namespace StateEstimation {

/**
 * @brief Dynamic-size Kalman filter alias using the templated implementation.
 *
 * This maintains the existing type name while consolidating code into a single
 * templated implementation that also supports fixed-size variants in tests.
 */
using KalmanFilter = KalmanFilterT<Eigen::Dynamic, Eigen::Dynamic>;

}// namespace StateEstimation

#endif// KALMAN_FILTER_HPP
