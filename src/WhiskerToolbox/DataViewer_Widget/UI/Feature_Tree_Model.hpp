#ifndef FEATURE_TREE_MODEL_HPP
#define FEATURE_TREE_MODEL_HPP

#include "DataManagerFwd.hpp"

#include <QObject>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class DataManager;

/**
 * @brief Model class for Feature_Tree_Widget following model-view architecture
 * 
 * Handles state management including:
 * - Color assignments for features
 * - Default color schemes by data type
 * - Feature state persistence
 * - Integration with DataManager
 */
class Feature_Tree_Model : public QObject {
    Q_OBJECT

public:
    explicit Feature_Tree_Model(QObject * parent = nullptr);
    ~Feature_Tree_Model() override = default;

    /**
     * @brief Set the DataManager instance
     * @param data_manager Shared pointer to DataManager
     */
    void setDataManager(std::shared_ptr<DataManager> data_manager);

    /**
     * @brief Get color for a specific feature
     * @param feature_key The feature key
     * @return Hex color string (e.g., "#FF0000")
     */
    [[nodiscard]] std::string getFeatureColor(std::string const & feature_key) const;

    /**
     * @brief Set color for a specific feature
     * @param feature_key The feature key
     * @param hex_color Hex color string (e.g., "#FF0000")
     */
    void setFeatureColor(std::string const & feature_key, std::string const & hex_color);

    /**
     * @brief Get default color for a data type
     * @param data_type The data type
     * @return Hex color string
     */
    [[nodiscard]] std::string getDefaultColorForType(DM_DataType data_type) const;

    /**
     * @brief Set enabled features (for state management)
     * @param feature_keys Vector of feature keys that are enabled
     */
    void setEnabledFeatures(std::vector<std::string> const & feature_keys);

    /**
     * @brief Get currently enabled features
     * @return Vector of enabled feature keys
     */
    [[nodiscard]] std::vector<std::string> getEnabledFeatures() const;

    /**
     * @brief Check if a feature is enabled
     * @param feature_key The feature key to check
     * @return True if enabled, false otherwise
     */
    [[nodiscard]] bool isFeatureEnabled(std::string const & feature_key) const;

signals:
    /**
     * @brief Emitted when feature colors change
     * @param feature_key The feature that changed color
     * @param hex_color The new color
     */
    void featureColorChanged(std::string const & feature_key, std::string const & hex_color);

private:
    std::shared_ptr<DataManager> _data_manager;
    
    // Color management
    std::unordered_map<std::string, std::string> _feature_colors;
    std::unordered_map<DM_DataType, std::string> _default_colors;
    
    // State management
    std::vector<std::string> _enabled_features;
    
    void _initializeDefaultColors();
};

#endif // FEATURE_TREE_MODEL_HPP
