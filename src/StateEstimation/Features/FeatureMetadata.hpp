#ifndef STATE_ESTIMATION_FEATURE_METADATA_HPP
#define STATE_ESTIMATION_FEATURE_METADATA_HPP

#include <string>
#include <functional>
#include <Eigen/Dense>

namespace StateEstimation {

/**
 * @brief Describes the temporal behavior of a feature
 * 
 * This enum classifies how features evolve over time and determines
 * how the state space is constructed for Kalman filtering.
 */
enum class FeatureTemporalType {
    /**
     * @brief Feature is time-invariant or slowly varying
     * 
     * Examples: line length, color, object class
     * State mapping: measurement [x] → state [x]
     * No velocity tracking.
     */
    STATIC,
    
    /**
     * @brief 2D kinematic feature with position and velocity
     * 
     * Examples: centroid position, base point position
     * State mapping: measurement [x, y] → state [x, y, vx, vy]
     */
    KINEMATIC_2D,
    
    /**
     * @brief 3D kinematic feature with position and velocity
     * 
     * Examples: 3D point tracking
     * State mapping: measurement [x, y, z] → state [x, y, z, vx, vy, vz]
     */
    KINEMATIC_3D,
    
    /**
     * @brief Scalar feature with first derivative
     * 
     * Examples: angle, length (if time-varying), curvature
     * State mapping: measurement [x] → state [x, dx/dt]
     */
    SCALAR_DYNAMIC,
    
    /**
     * @brief Custom state space mapping
     * 
     * For features requiring specialized state representations.
     * User must provide custom state transition matrices.
     */
    CUSTOM
};

/**
 * @brief Metadata describing a feature's characteristics
 * 
 * This structure provides all information needed to integrate a feature
 * into the tracking system, including its dimensionality, temporal behavior,
 * and how to construct the appropriate state space.
 */
struct FeatureMetadata {
    /// Human-readable name for the feature (e.g., "line_centroid")
    std::string name;
    
    /// Dimensionality of the measurement vector
    int measurement_size;
    
    /// Dimensionality of the state vector (typically >= measurement_size)
    int state_size;
    
    /// Type of temporal behavior
    FeatureTemporalType temporal_type;
    
    /**
     * @brief Calculate state size from measurement size and temporal type
     * 
     * Helper function to automatically determine the state size based on
     * the temporal type and measurement dimensionality.
     * 
     * @param measurement_size Dimensionality of the measurement
     * @param temporal_type Type of temporal behavior
     * @return Corresponding state size
     */
    static int calculateStateSize(int measurement_size, FeatureTemporalType temporal_type) {
        switch (temporal_type) {
            case FeatureTemporalType::STATIC:
                return measurement_size;  // No derivatives
            
            case FeatureTemporalType::KINEMATIC_2D:
                return 4;  // [x, y, vx, vy] - fixed size
            
            case FeatureTemporalType::KINEMATIC_3D:
                return 6;  // [x, y, z, vx, vy, vz] - fixed size
            
            case FeatureTemporalType::SCALAR_DYNAMIC:
                return 2 * measurement_size;  // Each scalar gets a derivative
            
            case FeatureTemporalType::CUSTOM:
                return measurement_size;  // Default, should be overridden
        }
        return measurement_size;
    }
    
    /**
     * @brief Construct metadata with automatic state size calculation
     * 
     * @param name Feature name
     * @param measurement_size Measurement dimensionality
     * @param temporal_type Temporal behavior type
     * @return FeatureMetadata with calculated state size
     */
    static FeatureMetadata create(std::string name, 
                                   int measurement_size, 
                                   FeatureTemporalType temporal_type) {
        return FeatureMetadata{
            .name = std::move(name),
            .measurement_size = measurement_size,
            .state_size = calculateStateSize(measurement_size, temporal_type),
            .temporal_type = temporal_type
        };
    }
    
    /**
     * @brief Check if this feature tracks derivatives (velocity, etc.)
     * 
     * @return true if state size > measurement size (derivatives present)
     */
    bool hasDerivatives() const {
        return state_size > measurement_size;
    }
    
    /**
     * @brief Get the order of derivatives tracked
     * 
     * @return 0 for STATIC, 1 for features with velocity, etc.
     */
    int getDerivativeOrder() const {
        switch (temporal_type) {
            case FeatureTemporalType::STATIC:
                return 0;
            case FeatureTemporalType::KINEMATIC_2D:
            case FeatureTemporalType::KINEMATIC_3D:
            case FeatureTemporalType::SCALAR_DYNAMIC:
                return 1;
            case FeatureTemporalType::CUSTOM:
                return (state_size - measurement_size) / measurement_size;
        }
        return 0;
    }
};

} // namespace StateEstimation

#endif // STATE_ESTIMATION_FEATURE_METADATA_HPP
