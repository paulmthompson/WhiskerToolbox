#ifndef STATE_ESTIMATION_KALMAN_MATRIX_BUILDER_HPP
#define STATE_ESTIMATION_KALMAN_MATRIX_BUILDER_HPP

#include <Eigen/Dense>
#include <vector>

namespace StateEstimation {

/**
 * @brief Utility class for building Kalman filter matrices for composite features
 * 
 * This class helps construct the state transition (F), measurement (H), process noise (Q),
 * and measurement noise (R) matrices when multiple features are tracked independently.
 * 
 * Each feature is assumed to have a 2D measurement space (e.g., x, y coordinates)
 * and a 4D state space (x, y, vx, vy - position + velocity).
 * 
 * For N features, the resulting matrices are:
 * - F: (4N × 4N) - Block diagonal with N copies of the 2D position + velocity model
 * - H: (2N × 4N) - Block diagonal with N copies of [I_2 | 0_2]
 * - Q: (4N × 4N) - Block diagonal with N copies of process noise
 * - R: (2N × 2N) - Block diagonal with N copies of measurement noise
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
};

} // namespace StateEstimation

#endif // STATE_ESTIMATION_KALMAN_MATRIX_BUILDER_HPP
