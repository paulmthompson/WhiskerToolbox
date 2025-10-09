#ifndef STATE_ESTIMATION_KALMAN_MATRIX_BUILDER_HPP
#define STATE_ESTIMATION_KALMAN_MATRIX_BUILDER_HPP

#include "Features/FeatureMetadata.hpp"

#include <Eigen/Dense>
#include <vector>

namespace StateEstimation {

/**
 * @brief Utility class for building Kalman filter matrices for composite features
 * 
 * This class constructs the state transition (F), measurement (H), process noise (Q),
 * and measurement noise (R) matrices for tracking systems with heterogeneous features.
 * 
 * Features can have different temporal behaviors:
 * - KINEMATIC_2D: 2D measurement (x,y) → 4D state (x, y, vx, vy)
 * - STATIC: 1D measurement → 1D state (no velocity)
 * - SCALAR_DYNAMIC: 1D measurement → 2D state (value + derivative)
 * 
 * The builder uses feature metadata to construct appropriate block-diagonal matrices
 * where each block corresponds to one feature with its specific dynamics.
 */
class KalmanMatrixBuilder {
public:
    /**
     * @brief Configuration for a single 2D feature (position + velocity model)
     */
    struct FeatureConfig {
        double dt = 1.0;                           // Time step
        double process_noise_position = 10.0;      // Process noise for position
        double process_noise_velocity = 1.0;       // Process noise for velocity
        double measurement_noise = 5.0;            // Measurement noise
        double static_noise_scale = 0.01;          // Multiplier for static features
    };
    
    /**
     * @brief Per-feature noise configuration
     * 
     * Allows different noise parameters for each feature in a composite system.
     */
    struct PerFeatureConfig {
        double dt = 1.0;
        double process_noise_position = 10.0;
        double process_noise_velocity = 1.0;
        double measurement_noise = 5.0;            // Used if feature-specific noise not provided
        double static_noise_scale = 0.01;
        
        // Feature-specific measurement noise (overrides default)
        std::map<std::string, double> feature_measurement_noise;
    };
    
    /**
     * @brief Build F matrix (state transition) for N features
     * 
     * Each feature block is:
     *   [1  0  dt  0]
     *   [0  1  0  dt]
     *   [0  0  1   0]
     *   [0  0  0   1]
     * 
     * @param configs Vector of feature configurations (one per feature)
     * @return Block-diagonal F matrix (4N × 4N)
     */
    static Eigen::MatrixXd buildF(std::vector<FeatureConfig> const& configs) {
        int num_features = static_cast<int>(configs.size());
        int state_size = 4 * num_features;
        
        Eigen::MatrixXd F = Eigen::MatrixXd::Zero(state_size, state_size);
        
        for (int i = 0; i < num_features; ++i) {
            double dt = configs[i].dt;
            int offset = 4 * i;
            
            // Position + velocity model for this feature
            F(offset + 0, offset + 0) = 1.0;
            F(offset + 0, offset + 2) = dt;
            F(offset + 1, offset + 1) = 1.0;
            F(offset + 1, offset + 3) = dt;
            F(offset + 2, offset + 2) = 1.0;
            F(offset + 3, offset + 3) = 1.0;
        }
        
        return F;
    }
    
    /**
     * @brief Build H matrix (measurement model) for N features
     * 
     * Each feature block is:
     *   [1  0  0  0]
     *   [0  1  0  0]
     * 
     * This extracts only the position components from the state.
     * 
     * @param num_features Number of features being tracked
     * @return Block-diagonal H matrix (2N × 4N)
     */
    static Eigen::MatrixXd buildH(int num_features) {
        int measurement_size = 2 * num_features;
        int state_size = 4 * num_features;
        
        Eigen::MatrixXd H = Eigen::MatrixXd::Zero(measurement_size, state_size);
        
        for (int i = 0; i < num_features; ++i) {
            int m_offset = 2 * i;
            int s_offset = 4 * i;
            
            // Extract position from state
            H(m_offset + 0, s_offset + 0) = 1.0;
            H(m_offset + 1, s_offset + 1) = 1.0;
        }
        
        return H;
    }
    
    /**
     * @brief Build Q matrix (process noise covariance) for N features
     * 
     * Each feature block is a 4×4 diagonal matrix:
     *   [σ_pos²    0        0         0     ]
     *   [0         σ_pos²   0         0     ]
     *   [0         0        σ_vel²    0     ]
     *   [0         0        0         σ_vel²]
     * 
     * @param configs Vector of feature configurations (one per feature)
     * @return Block-diagonal Q matrix (4N × 4N)
     */
    static Eigen::MatrixXd buildQ(std::vector<FeatureConfig> const& configs) {
        int num_features = static_cast<int>(configs.size());
        int state_size = 4 * num_features;
        
        Eigen::MatrixXd Q = Eigen::MatrixXd::Zero(state_size, state_size);
        
        for (int i = 0; i < num_features; ++i) {
            double pos_var = configs[i].process_noise_position * configs[i].process_noise_position;
            double vel_var = configs[i].process_noise_velocity * configs[i].process_noise_velocity;
            int offset = 4 * i;
            
            Q(offset + 0, offset + 0) = pos_var;
            Q(offset + 1, offset + 1) = pos_var;
            Q(offset + 2, offset + 2) = vel_var;
            Q(offset + 3, offset + 3) = vel_var;
        }
        
        return Q;
    }
    
    /**
     * @brief Build R matrix (measurement noise covariance) for N features
     * 
     * Each feature block is a 2×2 diagonal matrix:
     *   [σ_meas²    0      ]
     *   [0          σ_meas²]
     * 
     * @param configs Vector of feature configurations (one per feature)
     * @return Block-diagonal R matrix (2N × 2N)
     */
    static Eigen::MatrixXd buildR(std::vector<FeatureConfig> const& configs) {
        int num_features = static_cast<int>(configs.size());
        int measurement_size = 2 * num_features;
        
        Eigen::MatrixXd R = Eigen::MatrixXd::Zero(measurement_size, measurement_size);
        
        for (int i = 0; i < num_features; ++i) {
            double meas_var = configs[i].measurement_noise * configs[i].measurement_noise;
            int offset = 2 * i;
            
            R(offset + 0, offset + 0) = meas_var;
            R(offset + 1, offset + 1) = meas_var;
        }
        
        return R;
    }
    
    /**
     * @brief Build all matrices with the same configuration for all features
     * 
     * Convenience function when all features should use identical parameters.
     * 
     * @param num_features Number of features to track
     * @param config Configuration to use for all features
     * @return Tuple of (F, H, Q, R) matrices
     */
    static std::tuple<Eigen::MatrixXd, Eigen::MatrixXd, Eigen::MatrixXd, Eigen::MatrixXd>
    buildAllMatrices(int num_features, FeatureConfig const& config) {
        std::vector<FeatureConfig> configs(num_features, config);
        return {
            buildF(configs),
            buildH(num_features),
            buildQ(configs),
            buildR(configs)
        };
    }
    
    // ========================================================================
    // METADATA-BASED MATRIX BUILDERS
    // ========================================================================
    
    /**
     * @brief Build F matrix from feature metadata
     * 
     * Constructs a block-diagonal state transition matrix where each block
     * corresponds to one feature's temporal dynamics:
     * - KINEMATIC_2D/3D: Position + velocity model with dt
     * - STATIC: Identity (no change)
     * - SCALAR_DYNAMIC: Value + derivative model with dt
     * 
     * @param metadata_list Metadata for each feature in order
     * @param config Configuration (dt, noise parameters)
     * @return Block-diagonal F matrix
     */
    static Eigen::MatrixXd buildFFromMetadata(std::vector<FeatureMetadata> const& metadata_list,
                                               FeatureConfig const& config) {
        int total_state_size = 0;
        for (auto const& meta : metadata_list) {
            total_state_size += meta.state_size;
        }
        
        Eigen::MatrixXd F = Eigen::MatrixXd::Zero(total_state_size, total_state_size);
        
        int offset = 0;
        for (auto const& meta : metadata_list) {
            int state_size = meta.state_size;
            
            switch (meta.temporal_type) {
                case FeatureTemporalType::STATIC:
                    // Identity: state doesn't change
                    for (int i = 0; i < state_size; ++i) {
                        F(offset + i, offset + i) = 1.0;
                    }
                    break;
                
                case FeatureTemporalType::KINEMATIC_2D:
                    // 2D position + velocity: [x, y, vx, vy]
                    F(offset + 0, offset + 0) = 1.0;
                    F(offset + 0, offset + 2) = config.dt;
                    F(offset + 1, offset + 1) = 1.0;
                    F(offset + 1, offset + 3) = config.dt;
                    F(offset + 2, offset + 2) = 1.0;
                    F(offset + 3, offset + 3) = 1.0;
                    break;
                
                case FeatureTemporalType::KINEMATIC_3D:
                    // 3D position + velocity: [x, y, z, vx, vy, vz]
                    for (int i = 0; i < 3; ++i) {
                        F(offset + i, offset + i) = 1.0;
                        F(offset + i, offset + i + 3) = config.dt;
                        F(offset + i + 3, offset + i + 3) = 1.0;
                    }
                    break;
                
                case FeatureTemporalType::SCALAR_DYNAMIC:
                    // Each scalar gets: [value, derivative]
                    for (int i = 0; i < state_size / 2; ++i) {
                        F(offset + 2*i, offset + 2*i) = 1.0;
                        F(offset + 2*i, offset + 2*i + 1) = config.dt;
                        F(offset + 2*i + 1, offset + 2*i + 1) = 1.0;
                    }
                    break;
                
                case FeatureTemporalType::CUSTOM:
                    // Default to identity - user should override if needed
                    for (int i = 0; i < state_size; ++i) {
                        F(offset + i, offset + i) = 1.0;
                    }
                    break;
            }
            
            offset += state_size;
        }
        
        return F;
    }
    
    /**
     * @brief Build H matrix from feature metadata
     * 
     * Constructs a measurement matrix that extracts the measurement components
     * from the full state vector. For features with derivatives, this extracts
     * only the base values (not velocities).
     * 
     * @param metadata_list Metadata for each feature in order
     * @return Block-diagonal H matrix
     */
    static Eigen::MatrixXd buildHFromMetadata(std::vector<FeatureMetadata> const& metadata_list) {
        int total_measurement_size = 0;
        int total_state_size = 0;
        
        for (auto const& meta : metadata_list) {
            total_measurement_size += meta.measurement_size;
            total_state_size += meta.state_size;
        }
        
        Eigen::MatrixXd H = Eigen::MatrixXd::Zero(total_measurement_size, total_state_size);
        
        int m_offset = 0;
        int s_offset = 0;
        
        for (auto const& meta : metadata_list) {
            switch (meta.temporal_type) {
                case FeatureTemporalType::STATIC:
                    // Direct observation of state
                    for (int i = 0; i < meta.measurement_size; ++i) {
                        H(m_offset + i, s_offset + i) = 1.0;
                    }
                    break;
                
                case FeatureTemporalType::KINEMATIC_2D:
                    // Observe position, not velocity
                    H(m_offset + 0, s_offset + 0) = 1.0;
                    H(m_offset + 1, s_offset + 1) = 1.0;
                    break;
                
                case FeatureTemporalType::KINEMATIC_3D:
                    // Observe position, not velocity
                    H(m_offset + 0, s_offset + 0) = 1.0;
                    H(m_offset + 1, s_offset + 1) = 1.0;
                    H(m_offset + 2, s_offset + 2) = 1.0;
                    break;
                
                case FeatureTemporalType::SCALAR_DYNAMIC:
                    // Observe value, not derivative
                    for (int i = 0; i < meta.measurement_size; ++i) {
                        H(m_offset + i, s_offset + 2*i) = 1.0;
                    }
                    break;
                
                case FeatureTemporalType::CUSTOM:
                    // Default: observe first measurement_size components
                    for (int i = 0; i < meta.measurement_size; ++i) {
                        H(m_offset + i, s_offset + i) = 1.0;
                    }
                    break;
            }
            
            m_offset += meta.measurement_size;
            s_offset += meta.state_size;
        }
        
        return H;
    }
    
    /**
     * @brief Build Q matrix from feature metadata
     * 
     * Constructs process noise covariance. Features with derivatives get
     * noise for both the value and its rate of change.
     * 
     * @param metadata_list Metadata for each feature in order
     * @param config Configuration with noise parameters
     * @return Block-diagonal Q matrix
     */
    static Eigen::MatrixXd buildQFromMetadata(std::vector<FeatureMetadata> const& metadata_list,
                                               FeatureConfig const& config) {
        int total_state_size = 0;
        for (auto const& meta : metadata_list) {
            total_state_size += meta.state_size;
        }
        
        Eigen::MatrixXd Q = Eigen::MatrixXd::Zero(total_state_size, total_state_size);
        
        double pos_var = config.process_noise_position * config.process_noise_position;
        double vel_var = config.process_noise_velocity * config.process_noise_velocity;
        
        int offset = 0;
        for (auto const& meta : metadata_list) {
            switch (meta.temporal_type) {
                case FeatureTemporalType::STATIC:
                    // Small noise (nearly constant) - configurable scale
                    for (int i = 0; i < meta.state_size; ++i) {
                        Q(offset + i, offset + i) = config.static_noise_scale * pos_var;
                    }
                    break;
                
                case FeatureTemporalType::KINEMATIC_2D:
                    Q(offset + 0, offset + 0) = pos_var;
                    Q(offset + 1, offset + 1) = pos_var;
                    Q(offset + 2, offset + 2) = vel_var;
                    Q(offset + 3, offset + 3) = vel_var;
                    break;
                
                case FeatureTemporalType::KINEMATIC_3D:
                    for (int i = 0; i < 3; ++i) {
                        Q(offset + i, offset + i) = pos_var;
                        Q(offset + i + 3, offset + i + 3) = vel_var;
                    }
                    break;
                
                case FeatureTemporalType::SCALAR_DYNAMIC:
                    for (int i = 0; i < meta.state_size / 2; ++i) {
                        Q(offset + 2*i, offset + 2*i) = pos_var;
                        Q(offset + 2*i + 1, offset + 2*i + 1) = vel_var;
                    }
                    break;
                
                case FeatureTemporalType::CUSTOM:
                    // Default: moderate noise on all state components
                    for (int i = 0; i < meta.state_size; ++i) {
                        Q(offset + i, offset + i) = pos_var;
                    }
                    break;
            }
            
            offset += meta.state_size;
        }
        
        return Q;
    }
    
    /**
     * @brief Build R matrix from feature metadata
     * 
     * Constructs measurement noise covariance.
     * 
     * @param metadata_list Metadata for each feature in order
     * @param config Configuration with noise parameters
     * @return Block-diagonal R matrix
     */
    static Eigen::MatrixXd buildRFromMetadata(std::vector<FeatureMetadata> const& metadata_list,
                                               FeatureConfig const& config) {
        int total_measurement_size = 0;
        for (auto const& meta : metadata_list) {
            total_measurement_size += meta.measurement_size;
        }
        
        Eigen::MatrixXd R = Eigen::MatrixXd::Zero(total_measurement_size, total_measurement_size);
        
        double meas_var = config.measurement_noise * config.measurement_noise;
        
        int offset = 0;
        for (auto const& meta : metadata_list) {
            for (int i = 0; i < meta.measurement_size; ++i) {
                R(offset + i, offset + i) = meas_var;
            }
            offset += meta.measurement_size;
        }
        
        return R;
    }
    
    /**
     * @brief Build all matrices from metadata
     * 
     * Convenience function to build all four matrices at once.
     * 
     * @param metadata_list Metadata for each feature in order
     * @param config Configuration with dt and noise parameters
     * @return Tuple of (F, H, Q, R) matrices
     */
    static std::tuple<Eigen::MatrixXd, Eigen::MatrixXd, Eigen::MatrixXd, Eigen::MatrixXd>
    buildAllMatricesFromMetadata(std::vector<FeatureMetadata> const& metadata_list,
                                  FeatureConfig const& config) {
        return {
            buildFFromMetadata(metadata_list, config),
            buildHFromMetadata(metadata_list),
            buildQFromMetadata(metadata_list, config),
            buildRFromMetadata(metadata_list, config)
        };
    }
    
    /**
     * @brief Build R matrix with per-feature measurement noise
     * 
     * Allows different measurement noise for each feature based on name.
     * 
     * @param metadata_list Metadata for each feature in order
     * @param config Configuration with feature-specific noise map
     * @return Block-diagonal R matrix with per-feature noise
     */
    static Eigen::MatrixXd buildRFromMetadataPerFeature(
            std::vector<FeatureMetadata> const& metadata_list,
            PerFeatureConfig const& config) {
        int total_measurement_size = 0;
        for (auto const& meta : metadata_list) {
            total_measurement_size += meta.measurement_size;
        }
        
        Eigen::MatrixXd R = Eigen::MatrixXd::Zero(total_measurement_size, total_measurement_size);
        
        int offset = 0;
        for (auto const& meta : metadata_list) {
            // Check if feature has custom measurement noise
            double meas_noise = config.measurement_noise;  // Default
            auto it = config.feature_measurement_noise.find(meta.name);
            if (it != config.feature_measurement_noise.end()) {
                meas_noise = it->second;  // Use feature-specific noise
            }
            
            double meas_var = meas_noise * meas_noise;
            
            for (int i = 0; i < meta.measurement_size; ++i) {
                R(offset + i, offset + i) = meas_var;
            }
            offset += meta.measurement_size;
        }
        
        return R;
    }
    
    /**
     * @brief Build Q matrix with per-feature process noise
     * 
     * Uses configurable static noise scale for static features.
     * 
     * @param metadata_list Metadata for each feature in order
     * @param config Configuration with static noise scale
     * @return Block-diagonal Q matrix
     */
    static Eigen::MatrixXd buildQFromMetadataPerFeature(
            std::vector<FeatureMetadata> const& metadata_list,
            PerFeatureConfig const& config) {
        // Convert to old FeatureConfig for compatibility
        FeatureConfig fc;
        fc.dt = config.dt;
        fc.process_noise_position = config.process_noise_position;
        fc.process_noise_velocity = config.process_noise_velocity;
        fc.static_noise_scale = config.static_noise_scale;
        
        return buildQFromMetadata(metadata_list, fc);
    }
    
    /**
     * @brief Build all matrices with per-feature noise configuration
     * 
     * Allows different measurement noise for each feature type.
     * 
     * @param metadata_list Metadata for each feature in order
     * @param config Per-feature configuration
     * @return Tuple of (F, H, Q, R) matrices
     */
    static std::tuple<Eigen::MatrixXd, Eigen::MatrixXd, Eigen::MatrixXd, Eigen::MatrixXd>
    buildAllMatricesFromMetadataPerFeature(
            std::vector<FeatureMetadata> const& metadata_list,
            PerFeatureConfig const& config) {
        FeatureConfig fc;
        fc.dt = config.dt;
        fc.process_noise_position = config.process_noise_position;
        fc.process_noise_velocity = config.process_noise_velocity;
        fc.static_noise_scale = config.static_noise_scale;
        
        return {
            buildFFromMetadata(metadata_list, fc),
            buildHFromMetadata(metadata_list),
            buildQFromMetadataPerFeature(metadata_list, config),
            buildRFromMetadataPerFeature(metadata_list, config)
        };
    }
    
    /**
     * @brief Add cross-feature covariance to Q matrix
     * 
     * Modifies a process noise covariance matrix to include off-diagonal terms
     * representing correlated process noise between features. This is useful when
     * features are known to covary, such as position and measured length when
     * camera clipping causes the measured length to depend on position.
     * 
     * @param Q The base Q matrix (typically block-diagonal)
     * @param metadata_list Metadata for each feature (to find state offsets)
     * @param cross_correlations Map of (feature_i, feature_j) -> correlation coefficient
     * @return Modified Q matrix with cross-feature covariance
     */
    static Eigen::MatrixXd addCrossFeatureProcessNoise(
            Eigen::MatrixXd Q,
            std::vector<FeatureMetadata> const& metadata_list,
            std::map<std::pair<int, int>, double> const& cross_correlations) {
        
        if (cross_correlations.empty()) {
            return Q;
        }
        
        // Calculate feature state offsets
        std::vector<int> feature_state_offsets;
        int offset = 0;
        for (auto const& meta : metadata_list) {
            feature_state_offsets.push_back(offset);
            offset += meta.state_size;
        }
        
        // Apply cross-correlations
        for (auto const& [feature_pair, correlation] : cross_correlations) {
            int feat_i = feature_pair.first;
            int feat_j = feature_pair.second;
            
            if (feat_i >= static_cast<int>(metadata_list.size()) || 
                feat_j >= static_cast<int>(metadata_list.size())) {
                continue;  // Invalid indices
            }
            
            auto const& meta_i = metadata_list[feat_i];
            auto const& meta_j = metadata_list[feat_j];
            
            int offset_i = feature_state_offsets[feat_i];
            int offset_j = feature_state_offsets[feat_j];
            
            // Apply correlation to position components
            int pos_dim_i = (meta_i.temporal_type == FeatureTemporalType::KINEMATIC_2D) ? 2 : 
                           (meta_i.temporal_type == FeatureTemporalType::KINEMATIC_3D) ? 3 :
                           meta_i.measurement_size;
            int pos_dim_j = (meta_j.temporal_type == FeatureTemporalType::KINEMATIC_2D) ? 2 :
                           (meta_j.temporal_type == FeatureTemporalType::KINEMATIC_3D) ? 3 :
                           meta_j.measurement_size;
            
            for (int pi = 0; pi < pos_dim_i; ++pi) {
                for (int pj = 0; pj < pos_dim_j; ++pj) {
                    int si = offset_i + pi;
                    int sj = offset_j + pj;
                    
                    // Covariance = correlation * sqrt(var_i * var_j)
                    double std_i = std::sqrt(Q(si, si));
                    double std_j = std::sqrt(Q(sj, sj));
                    double cov = correlation * std_i * std_j;
                    
                    Q(si, sj) = cov;
                    Q(sj, si) = cov;  // Symmetric
                }
            }
        }
        
        return Q;
    }
};

} // namespace StateEstimation

#endif // STATE_ESTIMATION_KALMAN_MATRIX_BUILDER_HPP
