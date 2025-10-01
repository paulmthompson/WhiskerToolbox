#ifndef STATE_ESTIMATION_FEATURE_VECTOR_HPP
#define STATE_ESTIMATION_FEATURE_VECTOR_HPP

#include "Entity/EntityGroupManager.hpp"

#include <Eigen/Dense>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace StateEstimation {

// Note: GroupId is now imported from EntityGroupManager.hpp

/**
 * @brief Describes the type and properties of a feature
 */
enum class FeatureType {
    POSITION,   // Has meaningful derivatives (velocity, acceleration)
    ORIENTATION,// Angular values, may wrap around
    SCALE,      // Scalar values, typically positive
    INTENSITY,  // Intensity/brightness values
    SHAPE,      // Shape descriptors
    CUSTOM      // User-defined features
};

/**
 * @brief Metadata for a feature within a feature vector
 */
struct FeatureDescriptor {
    std::string name;       // Human-readable name
    FeatureType type;       // Type classification
    std::size_t start_index;// Starting index in the feature vector
    std::size_t size;       // Number of elements for this feature
    bool has_derivatives;   // Whether derivatives are meaningful

    FeatureDescriptor(std::string name, FeatureType type, std::size_t start_index,
                      std::size_t size, bool has_derivatives = true)
        : name(std::move(name)),
          type(type),
          start_index(start_index),
          size(size),
          has_derivatives(has_derivatives) {}
};

/**
 * @brief Container for multiple features with metadata
 * 
 * Stores feature values as a single Eigen vector while maintaining
 * metadata about individual features for proper handling during
 * tracking and assignment operations.
 */
class FeatureVector {
public:
    /**
     * @brief Construct empty feature vector
     */
    FeatureVector() = default;

    /**
     * @brief Construct with initial capacity
     */
    explicit FeatureVector(std::size_t initial_capacity);

    /**
     * @brief Add a feature to the vector
     * @param name Feature name
     * @param type Feature type classification
     * @param values Feature values
     * @param has_derivatives Whether derivatives are meaningful for this feature
     * @return Index of the added feature
     */
    std::size_t addFeature(std::string const & name, FeatureType type,
                           Eigen::VectorXd const & values, bool has_derivatives = true);

    /**
     * @brief Get the complete feature vector
     */
    [[nodiscard]] Eigen::VectorXd const & getVector() const { return values_; }

    /**
     * @brief Get mutable reference to feature vector
     */
    [[nodiscard]] Eigen::VectorXd & getVector() { return values_; }

    /**
     * @brief Get feature values by name
     */
    [[nodiscard]] Eigen::VectorXd getFeature(std::string const & name) const;

    /**
     * @brief Get feature values by index
     */
    [[nodiscard]] Eigen::VectorXd getFeature(std::size_t feature_index) const;

    /**
     * @brief Set feature values by name
     */
    void setFeature(std::string const & name, Eigen::VectorXd const & values);

    /**
     * @brief Set feature values by index
     */
    void setFeature(std::size_t feature_index, Eigen::VectorXd const & values);

    /**
     * @brief Get feature descriptor by name
     */
    [[nodiscard]] FeatureDescriptor const * getFeatureDescriptor(std::string const & name) const;

    /**
     * @brief Get feature descriptor by index
     */
    [[nodiscard]] FeatureDescriptor const & getFeatureDescriptor(std::size_t feature_index) const;

    /**
     * @brief Get all feature descriptors
     */
    [[nodiscard]] std::vector<FeatureDescriptor> const & getFeatureDescriptors() const { return descriptors_; }

    /**
     * @brief Get number of features
     */
    [[nodiscard]] std::size_t getFeatureCount() const { return descriptors_.size(); }

    /**
     * @brief Get total dimension of the feature vector
     */
    [[nodiscard]] std::size_t getDimension() const { return values_.size(); }

    /**
     * @brief Check if a feature exists
     */
    [[nodiscard]] bool hasFeature(std::string const & name) const;

    /**
     * @brief Clear all features
     */
    void clear();

    /**
     * @brief Create a copy with only specified features
     */
    [[nodiscard]] FeatureVector subset(std::vector<std::string> const & feature_names) const;

private:
    Eigen::VectorXd values_;                                    // All feature values concatenated
    std::vector<FeatureDescriptor> descriptors_;                // Metadata for each feature
    std::unordered_map<std::string, std::size_t> name_to_index_;// Fast lookup by name

    void rebuildNameIndex();
};

/**
 * @brief Abstract interface for extracting features from data objects
 * @tparam DataType The type of data object to extract features from
 */
template<typename DataType>
class FeatureExtractor {
public:
    virtual ~FeatureExtractor() = default;

    /**
     * @brief Extract features from a data object
     * @param data The data object to extract features from
     * @return FeatureVector containing extracted features
     */
    virtual FeatureVector extractFeatures(DataType const & data) const = 0;

    /**
     * @brief Get the names of features this extractor produces
     */
    virtual std::vector<std::string> getFeatureNames() const = 0;

    /**
     * @brief Get the expected dimension of the feature vector
     */
    virtual std::size_t getFeatureDimension() const = 0;
};

}// namespace StateEstimation

#endif// STATE_ESTIMATION_FEATURE_VECTOR_HPP