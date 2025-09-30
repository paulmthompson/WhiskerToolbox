#include "FeatureVector.hpp"
#include <stdexcept>
#include <algorithm>

namespace StateEstimation {

FeatureVector::FeatureVector(std::size_t initial_capacity) {
    values_.resize(initial_capacity);
    descriptors_.reserve(initial_capacity / 2); // Rough estimate of features
}

std::size_t FeatureVector::addFeature(std::string const& name, FeatureType type, 
                                     Eigen::VectorXd const& values, bool has_derivatives) {
    if (hasFeature(name)) {
        throw std::invalid_argument("Feature '" + name + "' already exists");
    }
    
    std::size_t start_index = values_.size();
    std::size_t feature_size = values.size();
    
    // Resize the main vector to accommodate new feature
    values_.conservativeResize(values_.size() + feature_size);
    values_.tail(feature_size) = values;
    
    // Add descriptor
    descriptors_.emplace_back(name, type, start_index, feature_size, has_derivatives);
    std::size_t feature_index = descriptors_.size() - 1;
    
    // Update name index
    name_to_index_[name] = feature_index;
    
    return feature_index;
}

Eigen::VectorXd FeatureVector::getFeature(std::string const& name) const {
    auto it = name_to_index_.find(name);
    if (it == name_to_index_.end()) {
        throw std::invalid_argument("Feature '" + name + "' not found");
    }
    return getFeature(it->second);
}

Eigen::VectorXd FeatureVector::getFeature(std::size_t feature_index) const {
    if (feature_index >= descriptors_.size()) {
        throw std::out_of_range("Feature index out of range");
    }
    
    auto const& desc = descriptors_[feature_index];
    return values_.segment(desc.start_index, desc.size);
}

void FeatureVector::setFeature(std::string const& name, Eigen::VectorXd const& values) {
    auto it = name_to_index_.find(name);
    if (it == name_to_index_.end()) {
        throw std::invalid_argument("Feature '" + name + "' not found");
    }
    setFeature(it->second, values);
}

void FeatureVector::setFeature(std::size_t feature_index, Eigen::VectorXd const& values) {
    if (feature_index >= descriptors_.size()) {
        throw std::out_of_range("Feature index out of range");
    }
    
    auto const& desc = descriptors_[feature_index];
    if (values.size() != static_cast<Eigen::Index>(desc.size)) {
        throw std::invalid_argument("Feature value size mismatch");
    }
    
    values_.segment(desc.start_index, desc.size) = values;
}

FeatureDescriptor const* FeatureVector::getFeatureDescriptor(std::string const& name) const {
    auto it = name_to_index_.find(name);
    if (it == name_to_index_.end()) {
        return nullptr;
    }
    return &descriptors_[it->second];
}

FeatureDescriptor const& FeatureVector::getFeatureDescriptor(std::size_t feature_index) const {
    if (feature_index >= descriptors_.size()) {
        throw std::out_of_range("Feature index out of range");
    }
    return descriptors_[feature_index];
}

bool FeatureVector::hasFeature(std::string const& name) const {
    return name_to_index_.find(name) != name_to_index_.end();
}

void FeatureVector::clear() {
    values_.resize(0);
    descriptors_.clear();
    name_to_index_.clear();
}

FeatureVector FeatureVector::subset(std::vector<std::string> const& feature_names) const {
    FeatureVector result;
    
    for (auto const& name : feature_names) {
        if (hasFeature(name)) {
            auto const* desc = getFeatureDescriptor(name);
            Eigen::VectorXd feature_values = getFeature(name);
            result.addFeature(desc->name, desc->type, feature_values, desc->has_derivatives);
        }
    }
    
    return result;
}

void FeatureVector::rebuildNameIndex() {
    name_to_index_.clear();
    for (std::size_t i = 0; i < descriptors_.size(); ++i) {
        name_to_index_[descriptors_[i].name] = i;
    }
}

} // namespace StateEstimation