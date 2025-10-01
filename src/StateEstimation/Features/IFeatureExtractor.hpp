#ifndef IFEATURE_EXTRACTOR_HPP
#define IFEATURE_EXTRACTOR_HPP

#include "Common.hpp"

#include <memory>
#include <string>

namespace StateEstimation {

/**
 * @brief Interface for extracting feature vectors from raw data types.
 *
 * This class abstracts the conversion of a specific data type (e.g., Line2D)
 * into generic feature vectors that the filter and assigner can use.
 *
 * @tparam DataType The raw data type to extract features from.
 */
template<typename DataType>
class IFeatureExtractor {
public:
    virtual ~IFeatureExtractor() = default;

    /**
     * @brief Extracts the feature vector used by the filter.
     * @param data The raw data object.
     * @return An Eigen::VectorXd containing features for the filter.
     */
    virtual Eigen::VectorXd getFilterFeatures(const DataType& data) const = 0;

    /**
     * @brief Extracts all available features for caching.
     * @param data The raw data object.
     * @return A FeatureCache map with all calculated features.
     */
    virtual FeatureCache getAllFeatures(const DataType& data) const = 0;
    
    /**
     * @brief Gets the name of the feature set used by the filter.
     * @return A string identifier for the filter features in the cache.
     */
    virtual std::string getFilterFeatureName() const = 0;

    /**
     * @brief Creates an initial FilterState from the first ground-truth measurement.
     * This is responsible for creating a full state vector (e.g., with zero velocity).
     * @param data The raw data object.
     * @return The initial FilterState.
     */
    virtual FilterState getInitialState(const DataType& data) const = 0;

    /**
     * @brief Clones the feature extractor.
     * @return A unique_ptr to a copy of this object.
     */
    virtual std::unique_ptr<IFeatureExtractor<DataType>> clone() const = 0;
};

} // namespace StateEstimation

#endif // IFEATURE_EXTRACTOR_HPP
