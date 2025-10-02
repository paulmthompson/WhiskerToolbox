#ifndef STATE_ESTIMATION_COMPOSITE_FEATURE_EXTRACTOR_HPP
#define STATE_ESTIMATION_COMPOSITE_FEATURE_EXTRACTOR_HPP

#include "IFeatureExtractor.hpp"

#include <Eigen/Dense>
#include <memory>
#include <string>
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
    
private:
    std::vector<std::unique_ptr<IFeatureExtractor<DataType>>> extractors_;
};

} // namespace StateEstimation

#endif // STATE_ESTIMATION_COMPOSITE_FEATURE_EXTRACTOR_HPP
