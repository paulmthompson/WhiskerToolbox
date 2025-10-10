#ifndef STATE_ESTIMATION_COMPOSITE_FEATURE_EXTRACTOR_HPP
#define STATE_ESTIMATION_COMPOSITE_FEATURE_EXTRACTOR_HPP

#include "IFeatureExtractor.hpp"

#include <Eigen/Dense>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace StateEstimation {

/**
 * @brief Feature extractor that chains multiple extractors together
 * 
 * This extractor allows combining multiple feature extractors to produce
 * a concatenated feature vector. The extractors are applied in the order
 * they are added, and their outputs are concatenated together.
 * 
 * The composite respects each feature's temporal behavior metadata:
 * - KINEMATIC_2D features: 2D measurement → 4D state (position + velocity)
 * - STATIC features: 1D measurement → 1D state (no velocity)
 * - SCALAR_DYNAMIC features: 1D measurement → 2D state (value + derivative)
 * 
 * Example: Combining centroid (KINEMATIC_2D) + length (STATIC):
 *   Measurements: [x_centroid, y_centroid, length] (3D)
 *   State: [x, y, vx, vy, length] (5D)
 * 
 * The initial state is constructed by concatenating the states from each extractor
 * and combining their covariances into a block-diagonal matrix.
 * 
 * @tparam DataType The raw data type to extract features from (e.g., Line2D)
 */
template<typename DataType>
class CompositeFeatureExtractor : public IFeatureExtractor<DataType> {
public:
    /**
     * @brief Construct an empty composite extractor
     */
    CompositeFeatureExtractor() = default;
    
    /**
     * @brief Construct a composite extractor from a list of extractors
     * 
     * @param extractors Vector of unique_ptrs to feature extractors (ownership transferred)
     */
    explicit CompositeFeatureExtractor(std::vector<std::unique_ptr<IFeatureExtractor<DataType>>> extractors)
        : extractors_(std::move(extractors)) {}
    
    /**
     * @brief Add a feature extractor to the chain
     * 
     * Extractors are applied in the order they are added.
     * 
     * @param extractor Unique_ptr to the extractor to add (ownership transferred)
     */
    void addExtractor(std::unique_ptr<IFeatureExtractor<DataType>> extractor) {
        extractors_.push_back(std::move(extractor));
    }
    
    /**
     * @brief Extract concatenated filter features from all extractors
     * 
     * Applies each extractor in order and concatenates their outputs.
     * 
     * @param data The raw data object to extract features from
     * @return Concatenated feature vector from all extractors
     */
    Eigen::VectorXd getFilterFeatures(DataType const& data) const override {
        if (extractors_.empty()) {
            return Eigen::VectorXd(0);
        }
        
        // Calculate total size needed
        int total_size = 0;
        for (auto const& extractor : extractors_) {
            total_size += extractor->getFilterFeatures(data).size();
        }
        
        // Concatenate features
        Eigen::VectorXd result(total_size);
        int offset = 0;
        for (auto const& extractor : extractors_) {
            Eigen::VectorXd features = extractor->getFilterFeatures(data);
            result.segment(offset, features.size()) = features;
            offset += features.size();
        }
        
        return result;
    }
    
    /**
     * @brief Extract all features from all extractors
     * 
     * Creates a unified cache with features from all extractors.
     * The composite feature is stored under "composite_features".
     * Individual extractor features are also stored under their original names.
     * 
     * @param data The raw data object to extract features from
     * @return FeatureCache with all features from all extractors
     */
    FeatureCache getAllFeatures(DataType const& data) const override {
        FeatureCache cache;
        
        // Add the composite filter features
        cache[getFilterFeatureName()] = getFilterFeatures(data);
        
        // Add individual extractor features
        for (auto const& extractor : extractors_) {
            auto extractor_cache = extractor->getAllFeatures(data);
            for (auto const& [key, value] : extractor_cache) {
                cache[key] = value;
            }
        }
        
        return cache;
    }
    
    /**
     * @brief Get the name identifier for composite filter features
     * 
     * @return "composite_features"
     */
    std::string getFilterFeatureName() const override {
        return "composite_features";
    }
    
    /**
     * @brief Create initial filter state from first observation
     * 
     * Concatenates the initial states from all extractors and creates
     * a block-diagonal covariance matrix.
     * 
     * Example with 2 extractors (each 4D state: [x, y, vx, vy]):
     *   Result state: [x1, y1, vx1, vy1, x2, y2, vx2, vy2] (8D)
     *   Result covariance: Block diagonal with 4×4 blocks from each extractor
     * 
     * @param data The raw data object to initialize from
     * @return FilterState with concatenated state and block-diagonal covariance
     */
    FilterState getInitialState(DataType const& data) const override {
        if (extractors_.empty()) {
            return FilterState{
                .state_mean = Eigen::VectorXd(0),
                .state_covariance = Eigen::MatrixXd(0, 0)
            };
        }
        
        // Get initial states from all extractors
        std::vector<FilterState> individual_states;
        int total_state_size = 0;
        
        for (auto const& extractor : extractors_) {
            auto state = extractor->getInitialState(data);
            individual_states.push_back(state);
            total_state_size += state.state_mean.size();
        }
        
        // Concatenate state means
        Eigen::VectorXd combined_mean(total_state_size);
        int offset = 0;
        for (auto const& state : individual_states) {
            int size = state.state_mean.size();
            combined_mean.segment(offset, size) = state.state_mean;
            offset += size;
        }
        
        // Create block-diagonal covariance matrix
        Eigen::MatrixXd combined_cov = Eigen::MatrixXd::Zero(total_state_size, total_state_size);
        offset = 0;
        for (auto const& state : individual_states) {
            int size = state.state_covariance.rows();
            combined_cov.block(offset, offset, size, size) = state.state_covariance;
            offset += size;
        }
        
        return FilterState{
            .state_mean = combined_mean,
            .state_covariance = combined_cov
        };
    }
    
    /**
     * @brief Clone this composite extractor
     * 
     * Creates a deep copy with cloned versions of all child extractors.
     * 
     * @return A unique_ptr to a copy of this extractor
     */
    std::unique_ptr<IFeatureExtractor<DataType>> clone() const override {
        std::vector<std::unique_ptr<IFeatureExtractor<DataType>>> cloned_extractors;
        for (auto const& extractor : extractors_) {
            cloned_extractors.push_back(extractor->clone());
        }
        return std::make_unique<CompositeFeatureExtractor<DataType>>(std::move(cloned_extractors));
    }
    
    /**
     * @brief Get the number of extractors in this composite
     * 
     * @return Number of child extractors
     */
    size_t getExtractorCount() const {
        return extractors_.size();
    }
    
    /**
     * @brief Get metadata for the composite feature
     * 
     * Creates aggregate metadata by combining information from all child extractors.
     * The measurement size is the sum of all child measurement sizes.
     * The state size is the sum of all child state sizes.
     * Type is marked as CUSTOM since it's a composition of multiple types.
     * 
     * @return FeatureMetadata describing the composite
     */
    FeatureMetadata getMetadata() const override {
        int total_measurement_size = 0;
        int total_state_size = 0;
        
        for (auto const& extractor : extractors_) {
            auto metadata = extractor->getMetadata();
            total_measurement_size += metadata.measurement_size;
            total_state_size += metadata.state_size;
        }
        
        return FeatureMetadata{
            .name = "composite_features",
            .measurement_size = total_measurement_size,
            .state_size = total_state_size,
            .temporal_type = FeatureTemporalType::CUSTOM
        };
    }
    
    /**
     * @brief Get metadata for all child extractors
     * 
     * Useful for building Kalman matrices with proper structure.
     * 
     * @return Vector of metadata from each child extractor in order
     */
    std::vector<FeatureMetadata> getChildMetadata() const {
        std::vector<FeatureMetadata> metadata_list;
        for (auto const& extractor : extractors_) {
            metadata_list.push_back(extractor->getMetadata());
        }
        return metadata_list;
    }
    
    /**
     * @brief Configuration for cross-feature covariance in initial state
     * 
     * Allows modeling correlations between different features, e.g., when
     * a static feature (length) correlates with position due to measurement
     * artifacts like camera clipping.
     */
    struct CrossCovarianceConfig {
        /// Correlation coefficient between features (-1 to 1)
        /// Example: position-length correlation when camera clips
        std::map<std::pair<int, int>, double> feature_correlations;
        
        /// State-level covariance entries (for fine-grained control)
        /// Maps (state_index_1, state_index_2) -> covariance value
        std::map<std::pair<int, int>, double> state_covariances;
    };
    
    /**
     * @brief Set cross-feature covariance configuration
     * 
     * This allows the initial state covariance to include off-diagonal terms
     * modeling known correlations between features.
     * 
     * Example: If position (feature 0) affects measured length (feature 2):
     *   config.feature_correlations[{0, 2}] = 0.3;  // 30% correlation
     * 
     * @param config Cross-covariance configuration
     */
    void setCrossCovarianceConfig(CrossCovarianceConfig config) {
        cross_cov_config_ = std::move(config);
    }
    
    /**
     * @brief Create initial filter state with optional cross-feature covariance
     * 
     * Extends the base implementation to add cross-feature covariance terms
     * based on the configured correlations. This allows modeling cases where
     * features are statistically dependent, such as when camera clipping causes
     * measured length to correlate with position.
     * 
     * @param data The raw data object to initialize from
     * @return FilterState with full covariance (including off-diagonal terms)
     */
    FilterState getInitialStateWithCrossCovariance(DataType const& data) const {
        // Get base state with block-diagonal covariance
        FilterState base_state = getInitialState(data);
        
        if (cross_cov_config_.feature_correlations.empty() && 
            cross_cov_config_.state_covariances.empty()) {
            return base_state;  // No cross-covariance requested
        }
        
        // Apply cross-feature correlations
        auto metadata_list = getChildMetadata();
        std::vector<int> feature_state_offsets;
        int offset = 0;
        for (auto const& meta : metadata_list) {
            feature_state_offsets.push_back(offset);
            offset += meta.state_size;
        }
        
        // Add feature-level correlations (position components of different features)
        for (auto const& [feature_pair, correlation] : cross_cov_config_.feature_correlations) {
            int feat_i = feature_pair.first;
            int feat_j = feature_pair.second;
            
            if (feat_i >= static_cast<int>(metadata_list.size()) || 
                feat_j >= static_cast<int>(metadata_list.size())) {
                continue;  // Invalid feature indices
            }
            
            auto const& meta_i = metadata_list[feat_i];
            auto const& meta_j = metadata_list[feat_j];
            
            int offset_i = feature_state_offsets[feat_i];
            int offset_j = feature_state_offsets[feat_j];
            
            // Apply correlation to position components
            // For KINEMATIC features: position is first components
            // For STATIC features: the value itself is the position
            int pos_dim_i = (meta_i.temporal_type == FeatureTemporalType::KINEMATIC_2D) ? 2 : meta_i.measurement_size;
            int pos_dim_j = (meta_j.temporal_type == FeatureTemporalType::KINEMATIC_2D) ? 2 : meta_j.measurement_size;
            
            for (int pi = 0; pi < pos_dim_i; ++pi) {
                for (int pj = 0; pj < pos_dim_j; ++pj) {
                    int si = offset_i + pi;
                    int sj = offset_j + pj;
                    
                    // Covariance = correlation * sqrt(var_i * var_j)
                    double std_i = std::sqrt(base_state.state_covariance(si, si));
                    double std_j = std::sqrt(base_state.state_covariance(sj, sj));
                    double cov = correlation * std_i * std_j;
                    
                    base_state.state_covariance(si, sj) = cov;
                    base_state.state_covariance(sj, si) = cov;  // Symmetric
                }
            }
        }
        
        // Add explicit state-level covariances
        for (auto const& [state_pair, cov_value] : cross_cov_config_.state_covariances) {
            int si = state_pair.first;
            int sj = state_pair.second;
            
            if (si < base_state.state_covariance.rows() && 
                sj < base_state.state_covariance.cols()) {
                base_state.state_covariance(si, sj) = cov_value;
                base_state.state_covariance(sj, si) = cov_value;  // Symmetric
            }
        }
        
        return base_state;
    }
    
private:
    std::vector<std::unique_ptr<IFeatureExtractor<DataType>>> extractors_;
    CrossCovarianceConfig cross_cov_config_;
};

} // namespace StateEstimation

#endif // STATE_ESTIMATION_COMPOSITE_FEATURE_EXTRACTOR_HPP
