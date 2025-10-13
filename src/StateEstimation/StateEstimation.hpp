#ifndef STATE_ESTIMATION_HPP
#define STATE_ESTIMATION_HPP

#include <Eigen/Dense>

// Core feature framework
#include "Features/FeatureVector.hpp"
#include "Features/LineFeatureExtractor.hpp"

// Assignment algorithms
#include "Assignment/AssignmentProblem.hpp"
#include "Assignment/hungarian.hpp"

// Tracking components
#include "Tracking/MultiFeatureKalman.hpp"
#include "Filter/Kalman/kalman.hpp"

#endif