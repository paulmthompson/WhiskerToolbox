#ifndef STATE_ESTIMATION_MULTI_FEATURE_KALMAN_HPP
#define STATE_ESTIMATION_MULTI_FEATURE_KALMAN_HPP

#include "Features/FeatureVector.hpp"
#include "Filter/Kalman/kalman.hpp"

#include <Eigen/Dense>

#include <memory>
#include <unordered_map>

namespace StateEstimation {

/**
 * @brief Configuration for a multi-feature Kalman filter
 */
struct MultiFeatureKalmanConfig {
    double dt = 1.0;                          // Time step
    double default_process_noise = 1.0;       // Default process noise for features
    double default_measurement_noise = 1.0;   // Default measurement noise
    double default_initial_uncertainty = 10.0;// Default initial state uncertainty

    // Feature-specific noise parameters
    std::unordered_map<std::string, double> feature_process_noise;
    std::unordered_map<std::string, double> feature_measurement_noise;
    std::unordered_map<std::string, double> feature_initial_uncertainty;

    // Whether to include derivatives (velocity, acceleration) for each feature type
    std::unordered_map<FeatureType, bool> include_derivatives{
            {FeatureType::POSITION, true},
            {FeatureType::ORIENTATION, false},
            {FeatureType::SCALE, false},
            {FeatureType::INTENSITY, false},
            {FeatureType::SHAPE, false},
            {FeatureType::CUSTOM, false}};
};

/**
 * @brief Prediction result from multi-feature Kalman filter
 */
struct FeaturePrediction {
    FeatureVector predicted_features;// Predicted feature values
    Eigen::MatrixXd covariance;      // Prediction covariance matrix
    double confidence;               // Confidence score (0-1)
    bool valid;                      // Whether prediction is valid

    FeaturePrediction()
        : confidence(0.0),
          valid(false) {}
};

/**
 * @brief Multi-feature Kalman filter for tracking arbitrary feature sets
 * 
 * This class extends the basic Kalman filter to handle multiple features with
 * different types and properties. It automatically constructs state vectors
 * that include derivatives for features where they are meaningful.
 */
class MultiFeatureKalmanFilter {
public:
    /**
     * @brief Construct with configuration
     */
    explicit MultiFeatureKalmanFilter(MultiFeatureKalmanConfig config = {});

    /**
     * @brief Initialize filter with initial feature observation
     * @param initial_features Initial feature observation
     * @param initial_time Initial time
     */
    void initialize(FeatureVector const & initial_features, double initial_time = 0.0);

    /**
     * @brief Check if filter is initialized and ready for prediction/update
     */
    [[nodiscard]] bool isInitialized() const;

    /**
     * @brief Predict features at next time step
     * @param dt Time step (uses config dt if not specified)
     * @return Predicted features with uncertainty
     */
    [[nodiscard]] FeaturePrediction predict(double dt = -1.0);

    /**
     * @brief Update filter with new feature observation
     * @param observed_features New feature observation
     * @param dt Time step since last update (uses config dt if not specified)
     */
    void update(FeatureVector const & observed_features, double dt = -1.0);

    /**
     * @brief Get current state as feature vector (positions only, no derivatives)
     */
    [[nodiscard]] FeatureVector getCurrentFeatures() const;

    /**
     * @brief Get current full state vector (including derivatives)
     */
    [[nodiscard]] Eigen::VectorXd getCurrentState() const;

    /**
     * @brief Get current state covariance matrix
     */
    [[nodiscard]] Eigen::MatrixXd getCurrentCovariance() const;

    /**
     * @brief Get current time
     */
    [[nodiscard]] double getCurrentTime() const;

    /**
     * @brief Reset filter to uninitialized state
     */
    void reset();

    /**
     * @brief Update configuration (requires reinitialization)
     */
    void setConfig(MultiFeatureKalmanConfig config);

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] MultiFeatureKalmanConfig const & getConfig() const { return config_; }

    /**
     * @brief Get feature template used for state construction
     */
    [[nodiscard]] FeatureVector const & getFeatureTemplate() const { return feature_template_; }

private:
    MultiFeatureKalmanConfig config_;
    FeatureVector feature_template_;// Template defining feature structure
    std::unique_ptr<KalmanFilter> kalman_filter_;
    bool initialized_;

    // State vector organization
    struct StateMapping {
        std::size_t position_start;// Start index for position values
        std::size_t velocity_start;// Start index for velocity values (if applicable)
        std::size_t total_size;    // Total size for this feature in state vector
        bool has_derivatives;      // Whether this feature includes derivatives
    };

    std::vector<StateMapping> state_mappings_;// Maps feature index to state vector indices
    std::size_t total_state_size_;            // Total size of state vector
    std::size_t measurement_size_;            // Size of measurement vector (positions only)

    /**
     * @brief Build state vector mappings from feature template
     */
    void buildStateMappings();

    /**
     * @brief Create system matrices for Kalman filter
     */
    void createSystemMatrices();

    /**
     * @brief Create dynamics matrix for given time step
     */
    Eigen::MatrixXd createDynamicsMatrix(double dt) const;

    /**
     * @brief Convert feature vector to state vector (with derivatives set to zero)
     */
    Eigen::VectorXd featureVectorToState(FeatureVector const & features) const;

    /**
     * @brief Convert state vector to feature vector (extract positions only)
     */
    FeatureVector stateToFeatureVector(Eigen::VectorXd const & state) const;

    /**
     * @brief Convert feature vector to measurement vector
     */
    Eigen::VectorXd featureVectorToMeasurement(FeatureVector const & features) const;

    /**
     * @brief Get measurement covariance matrix
     */
    Eigen::MatrixXd getMeasurementCovariance() const;

    /**
     * @brief Calculate confidence from covariance matrix
     */
    double calculateConfidence(Eigen::MatrixXd const & covariance) const;

    /**
     * @brief Get noise parameter for a specific feature
     */
    double getFeatureProcessNoise(std::string const & feature_name) const;
    double getFeatureMeasurementNoise(std::string const & feature_name) const;
    double getFeatureInitialUncertainty(std::string const & feature_name) const;
};

/**
 * @brief Group tracker that manages multiple groups with multi-feature Kalman filters
 */
class GroupTracker {
public:
    /**
     * @brief Construct with configuration
     */
    explicit GroupTracker(MultiFeatureKalmanConfig config = {});

    /**
     * @brief Initialize tracking for a group
     * @param group_id Group identifier
     * @param initial_features Initial feature observation for the group
     * @param initial_time Initial time
     */
    void initializeGroup(GroupId group_id, FeatureVector const & initial_features, double initial_time = 0.0);

    /**
     * @brief Check if a group is being tracked
     */
    [[nodiscard]] bool isGroupTracked(GroupId group_id) const;

    /**
     * @brief Get prediction for a group
     * @param group_id Group identifier
     * @param dt Time step for prediction
     * @return Prediction result
     */
    [[nodiscard]] FeaturePrediction predictGroup(GroupId group_id, double dt = -1.0);

    /**
     * @brief Update a group with new observation
     * @param group_id Group identifier
     * @param observed_features New feature observation
     * @param dt Time step since last update
     */
    void updateGroup(GroupId group_id, FeatureVector const & observed_features, double dt = -1.0);

    /**
     * @brief Get current features for a group
     */
    [[nodiscard]] FeatureVector getCurrentFeatures(GroupId group_id) const;

    /**
     * @brief Remove a group from tracking
     */
    void removeGroup(GroupId group_id);

    /**
     * @brief Get all tracked group IDs
     */
    [[nodiscard]] std::vector<GroupId> getTrackedGroups() const;

    /**
     * @brief Reset all groups
     */
    void reset();

private:
    MultiFeatureKalmanConfig config_;
    std::unordered_map<GroupId, std::unique_ptr<MultiFeatureKalmanFilter>> group_filters_;
};

}// namespace StateEstimation

#endif// STATE_ESTIMATION_MULTI_FEATURE_KALMAN_HPP