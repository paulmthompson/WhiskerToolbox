#include "MultiFeatureKalman.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace StateEstimation {

// ========== MultiFeatureKalmanFilter Implementation ==========

MultiFeatureKalmanFilter::MultiFeatureKalmanFilter(MultiFeatureKalmanConfig config)
    : config_(std::move(config)),
      kalman_filter_(nullptr),
      initialized_(false),
      total_state_size_(0),
      measurement_size_(0) {}

void MultiFeatureKalmanFilter::initialize(FeatureVector const & initial_features, double initial_time) {
    feature_template_ = initial_features;
    buildStateMappings();
    createSystemMatrices();

    // Convert initial features to state vector
    Eigen::VectorXd initial_state = featureVectorToState(initial_features);

    kalman_filter_->init(initial_time, initial_state);
    initialized_ = true;
}

bool MultiFeatureKalmanFilter::isInitialized() const {
    return initialized_ && kalman_filter_ != nullptr;
}

FeaturePrediction MultiFeatureKalmanFilter::predict(double dt) {
    if (!isInitialized()) {
        return FeaturePrediction{};// Invalid prediction
    }

    double time_step = (dt > 0) ? dt : config_.dt;

    // Create temporary Kalman filter for prediction without affecting current state
    auto temp_filter = std::make_unique<KalmanFilter>(*kalman_filter_);

    // Update system matrix for this time step if different from default
    if (std::abs(time_step - config_.dt) > 1e-9) {
        Eigen::MatrixXd A = createDynamicsMatrix(time_step);

        // Dummy measurement for prediction step
        Eigen::VectorXd dummy_measurement = Eigen::VectorXd::Zero(measurement_size_);
        temp_filter->update(dummy_measurement, time_step, A);

        // Extract prediction without updating measurement
        Eigen::VectorXd predicted_state = temp_filter->state();
        predicted_state = kalman_filter_->state();// Restore original state

        // Manual prediction step
        A = createDynamicsMatrix(time_step);
        predicted_state = A * kalman_filter_->state();

        FeaturePrediction result;
        result.predicted_features = stateToFeatureVector(predicted_state);
        result.covariance = getMeasurementCovariance();// Simplified for now
        result.confidence = calculateConfidence(result.covariance);
        result.valid = true;

        return result;
    } else {
        // Use default prediction
        Eigen::VectorXd predicted_state = kalman_filter_->state();

        FeaturePrediction result;
        result.predicted_features = stateToFeatureVector(predicted_state);
        result.covariance = getMeasurementCovariance();
        result.confidence = calculateConfidence(result.covariance);
        result.valid = true;

        return result;
    }
}

void MultiFeatureKalmanFilter::update(FeatureVector const & observed_features, double dt) {
    if (!isInitialized()) {
        throw std::runtime_error("Filter must be initialized before update");
    }

    double time_step = (dt > 0) ? dt : config_.dt;

    // Convert features to measurement vector
    Eigen::VectorXd measurement = featureVectorToMeasurement(observed_features);

    if (std::abs(time_step - config_.dt) > 1e-9) {
        // Update with custom time step
        Eigen::MatrixXd A = createDynamicsMatrix(time_step);
        kalman_filter_->update(measurement, time_step, A);
    } else {
        // Use default update
        kalman_filter_->update(measurement);
    }
}

FeatureVector MultiFeatureKalmanFilter::getCurrentFeatures() const {
    if (!isInitialized()) {
        return FeatureVector{};
    }

    return stateToFeatureVector(kalman_filter_->state());
}

Eigen::VectorXd MultiFeatureKalmanFilter::getCurrentState() const {
    if (!isInitialized()) {
        return Eigen::VectorXd{};
    }

    return kalman_filter_->state();
}

Eigen::MatrixXd MultiFeatureKalmanFilter::getCurrentCovariance() const {
    if (!isInitialized()) {
        return Eigen::MatrixXd{};
    }

    // Note: Current KalmanFilter doesn't expose covariance matrix
    // For now, return measurement covariance as approximation
    return getMeasurementCovariance();
}

double MultiFeatureKalmanFilter::getCurrentTime() const {
    if (!isInitialized()) {
        return 0.0;
    }

    return kalman_filter_->time();
}

void MultiFeatureKalmanFilter::reset() {
    kalman_filter_.reset();
    initialized_ = false;
    feature_template_.clear();
    state_mappings_.clear();
    total_state_size_ = 0;
    measurement_size_ = 0;
}

void MultiFeatureKalmanFilter::setConfig(MultiFeatureKalmanConfig config) {
    config_ = std::move(config);
    if (initialized_) {
        // Require reinitialization after config change
        reset();
    }
}

void MultiFeatureKalmanFilter::buildStateMappings() {
    state_mappings_.clear();
    total_state_size_ = 0;
    measurement_size_ = 0;

    for (std::size_t i = 0; i < feature_template_.getFeatureCount(); ++i) {
        auto const & desc = feature_template_.getFeatureDescriptor(i);

        StateMapping mapping;
        mapping.position_start = total_state_size_;
        mapping.has_derivatives = desc.has_derivatives &&
                                  config_.include_derivatives.at(desc.type);

        if (mapping.has_derivatives) {
            mapping.velocity_start = mapping.position_start + desc.size;
            mapping.total_size = desc.size * 2;// Position + velocity
        } else {
            mapping.velocity_start = 0;    // Not used
            mapping.total_size = desc.size;// Position only
        }

        state_mappings_.push_back(mapping);
        total_state_size_ += mapping.total_size;
        measurement_size_ += desc.size;// Measurements are always position values
    }
}

void MultiFeatureKalmanFilter::createSystemMatrices() {
    // Create dynamics matrix (A)
    Eigen::MatrixXd A = createDynamicsMatrix(config_.dt);

    // Create measurement matrix (C) - extract positions from state
    Eigen::MatrixXd C = Eigen::MatrixXd::Zero(measurement_size_, total_state_size_);
    std::size_t measurement_idx = 0;

    for (std::size_t i = 0; i < state_mappings_.size(); ++i) {
        auto const & mapping = state_mappings_[i];
        auto const & desc = feature_template_.getFeatureDescriptor(i);

        // Extract position part of state
        C.block(measurement_idx, mapping.position_start, desc.size, desc.size) =
                Eigen::MatrixXd::Identity(desc.size, desc.size);

        measurement_idx += desc.size;
    }

    // Create process noise matrix (Q)
    Eigen::MatrixXd Q = Eigen::MatrixXd::Zero(total_state_size_, total_state_size_);
    std::size_t state_idx = 0;

    for (std::size_t i = 0; i < state_mappings_.size(); ++i) {
        auto const & mapping = state_mappings_[i];
        auto const & desc = feature_template_.getFeatureDescriptor(i);

        double pos_noise = getFeatureProcessNoise(desc.name);
        double vel_noise = pos_noise * 0.1;// Velocity noise typically smaller

        // Position noise
        for (std::size_t j = 0; j < desc.size; ++j) {
            Q(mapping.position_start + j, mapping.position_start + j) = pos_noise * pos_noise;
        }

        // Velocity noise (if applicable)
        if (mapping.has_derivatives) {
            for (std::size_t j = 0; j < desc.size; ++j) {
                Q(mapping.velocity_start + j, mapping.velocity_start + j) = vel_noise * vel_noise;
            }
        }
    }

    // Create measurement noise matrix (R)
    Eigen::MatrixXd R = Eigen::MatrixXd::Zero(measurement_size_, measurement_size_);
    measurement_idx = 0;

    for (std::size_t i = 0; i < feature_template_.getFeatureCount(); ++i) {
        auto const & desc = feature_template_.getFeatureDescriptor(i);
        double meas_noise = getFeatureMeasurementNoise(desc.name);

        for (std::size_t j = 0; j < desc.size; ++j) {
            R(measurement_idx + j, measurement_idx + j) = meas_noise * meas_noise;
        }

        measurement_idx += desc.size;
    }

    // Create initial covariance matrix (P)
    Eigen::MatrixXd P = Eigen::MatrixXd::Zero(total_state_size_, total_state_size_);

    for (std::size_t i = 0; i < state_mappings_.size(); ++i) {
        auto const & mapping = state_mappings_[i];
        auto const & desc = feature_template_.getFeatureDescriptor(i);

        double pos_unc = getFeatureInitialUncertainty(desc.name);
        double vel_unc = pos_unc * 0.5;// Velocity uncertainty typically smaller

        // Position uncertainty
        for (std::size_t j = 0; j < desc.size; ++j) {
            P(mapping.position_start + j, mapping.position_start + j) = pos_unc * pos_unc;
        }

        // Velocity uncertainty (if applicable)
        if (mapping.has_derivatives) {
            for (std::size_t j = 0; j < desc.size; ++j) {
                P(mapping.velocity_start + j, mapping.velocity_start + j) = vel_unc * vel_unc;
            }
        }
    }

    // Create Kalman filter
    kalman_filter_ = std::make_unique<KalmanFilter>(config_.dt, A, C, Q, R, P);
}

Eigen::MatrixXd MultiFeatureKalmanFilter::createDynamicsMatrix(double dt) const {
    Eigen::MatrixXd A = Eigen::MatrixXd::Identity(total_state_size_, total_state_size_);

    for (std::size_t i = 0; i < state_mappings_.size(); ++i) {
        auto const & mapping = state_mappings_[i];
        auto const & desc = feature_template_.getFeatureDescriptor(i);

        if (mapping.has_derivatives) {
            // Position = position + velocity * dt
            for (std::size_t j = 0; j < desc.size; ++j) {
                A(mapping.position_start + j, mapping.velocity_start + j) = dt;
            }
            // Velocity remains constant (velocity = velocity)
        }
        // For features without derivatives, state remains constant
    }

    return A;
}

Eigen::VectorXd MultiFeatureKalmanFilter::featureVectorToState(FeatureVector const & features) const {
    Eigen::VectorXd state = Eigen::VectorXd::Zero(total_state_size_);

    for (std::size_t i = 0; i < state_mappings_.size(); ++i) {
        auto const & mapping = state_mappings_[i];
        auto const & desc = feature_template_.getFeatureDescriptor(i);

        if (features.hasFeature(desc.name)) {
            Eigen::VectorXd feature_values = features.getFeature(desc.name);

            // Set position values
            state.segment(mapping.position_start, desc.size) = feature_values;

            // Initialize velocities to zero if applicable
            if (mapping.has_derivatives) {
                state.segment(mapping.velocity_start, desc.size).setZero();
            }
        }
    }

    return state;
}

FeatureVector MultiFeatureKalmanFilter::stateToFeatureVector(Eigen::VectorXd const & state) const {
    FeatureVector features;

    for (std::size_t i = 0; i < state_mappings_.size(); ++i) {
        auto const & mapping = state_mappings_[i];
        auto const & desc = feature_template_.getFeatureDescriptor(i);

        // Extract position values
        Eigen::VectorXd feature_values = state.segment(mapping.position_start, desc.size);
        features.addFeature(desc.name, desc.type, feature_values, desc.has_derivatives);
    }

    return features;
}

Eigen::VectorXd MultiFeatureKalmanFilter::featureVectorToMeasurement(FeatureVector const & features) const {
    Eigen::VectorXd measurement = Eigen::VectorXd::Zero(measurement_size_);
    std::size_t measurement_idx = 0;

    for (std::size_t i = 0; i < feature_template_.getFeatureCount(); ++i) {
        auto const & desc = feature_template_.getFeatureDescriptor(i);

        if (features.hasFeature(desc.name)) {
            Eigen::VectorXd feature_values = features.getFeature(desc.name);
            measurement.segment(measurement_idx, desc.size) = feature_values;
        }

        measurement_idx += desc.size;
    }

    return measurement;
}

Eigen::MatrixXd MultiFeatureKalmanFilter::getMeasurementCovariance() const {
    Eigen::MatrixXd R = Eigen::MatrixXd::Zero(measurement_size_, measurement_size_);
    std::size_t measurement_idx = 0;

    for (std::size_t i = 0; i < feature_template_.getFeatureCount(); ++i) {
        auto const & desc = feature_template_.getFeatureDescriptor(i);
        double meas_noise = getFeatureMeasurementNoise(desc.name);

        for (std::size_t j = 0; j < desc.size; ++j) {
            R(measurement_idx + j, measurement_idx + j) = meas_noise * meas_noise;
        }

        measurement_idx += desc.size;
    }

    return R;
}

double MultiFeatureKalmanFilter::calculateConfidence(Eigen::MatrixXd const & covariance) const {
    // Simple confidence measure based on trace of covariance matrix
    double trace = covariance.trace();
    double max_uncertainty = config_.default_initial_uncertainty * measurement_size_;

    // Confidence decreases as uncertainty increases
    double confidence = std::exp(-trace / max_uncertainty);
    return std::max(0.0, std::min(1.0, confidence));
}

double MultiFeatureKalmanFilter::getFeatureProcessNoise(std::string const & feature_name) const {
    auto it = config_.feature_process_noise.find(feature_name);
    return (it != config_.feature_process_noise.end()) ? it->second : config_.default_process_noise;
}

double MultiFeatureKalmanFilter::getFeatureMeasurementNoise(std::string const & feature_name) const {
    auto it = config_.feature_measurement_noise.find(feature_name);
    return (it != config_.feature_measurement_noise.end()) ? it->second : config_.default_measurement_noise;
}

double MultiFeatureKalmanFilter::getFeatureInitialUncertainty(std::string const & feature_name) const {
    auto it = config_.feature_initial_uncertainty.find(feature_name);
    return (it != config_.feature_initial_uncertainty.end()) ? it->second : config_.default_initial_uncertainty;
}

// ========== GroupTracker Implementation ==========

GroupTracker::GroupTracker(MultiFeatureKalmanConfig config)
    : config_(std::move(config)) {}

void GroupTracker::initializeGroup(GroupId group_id, FeatureVector const & initial_features, double initial_time) {
    auto filter = std::make_unique<MultiFeatureKalmanFilter>(config_);
    filter->initialize(initial_features, initial_time);
    group_filters_[group_id] = std::move(filter);
}

bool GroupTracker::isGroupTracked(GroupId group_id) const {
    auto it = group_filters_.find(group_id);
    return it != group_filters_.end() && it->second->isInitialized();
}

FeaturePrediction GroupTracker::predictGroup(GroupId group_id, double dt) {
    auto it = group_filters_.find(group_id);
    if (it == group_filters_.end() || !it->second->isInitialized()) {
        return FeaturePrediction{};// Invalid prediction
    }

    return it->second->predict(dt);
}

void GroupTracker::updateGroup(GroupId group_id, FeatureVector const & observed_features, double dt) {
    auto it = group_filters_.find(group_id);
    if (it == group_filters_.end() || !it->second->isInitialized()) {
        throw std::runtime_error("Group " + std::to_string(group_id) + " is not being tracked");
    }

    it->second->update(observed_features, dt);
}

FeatureVector GroupTracker::getCurrentFeatures(GroupId group_id) const {
    auto it = group_filters_.find(group_id);
    if (it == group_filters_.end() || !it->second->isInitialized()) {
        return FeatureVector{};
    }

    return it->second->getCurrentFeatures();
}

void GroupTracker::removeGroup(GroupId group_id) {
    group_filters_.erase(group_id);
}

std::vector<GroupId> GroupTracker::getTrackedGroups() const {
    std::vector<GroupId> groups;
    for (auto const & [group_id, filter]: group_filters_) {
        if (filter->isInitialized()) {
            groups.push_back(group_id);
        }
    }
    return groups;
}

void GroupTracker::reset() {
    group_filters_.clear();
}

}// namespace StateEstimation