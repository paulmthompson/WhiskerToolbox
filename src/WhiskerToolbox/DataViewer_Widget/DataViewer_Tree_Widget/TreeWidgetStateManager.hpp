#ifndef TREEWIDGETSTATEMANAGER_HPP
#define TREEWIDGETSTATEMANAGER_HPP

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <optional>
#include <iostream>

/**
 * @brief Manages the enabled/disabled state of series in the tree widget
 * 
 * This class is responsible for saving and restoring the toggle states
 * of series items in the DataViewer tree widget. It's separated from
 * the Qt widget logic to make it testable.
 */
class TreeWidgetStateManager {
public:
    TreeWidgetStateManager() = default;
    ~TreeWidgetStateManager() = default;
    
    /**
     * @brief Save the current enabled state of individual series
     * @param enabled_series Set of series keys that are currently enabled
     */
    void saveEnabledSeries(const std::unordered_set<std::string>& enabled_series);
    
    /**
     * @brief Save the current enabled state of groups
     * @param group_enabled_state Map of group names to their enabled state
     */
    void saveGroupStates(const std::unordered_map<std::string, bool>& group_enabled_state);
    
    /**
     * @brief Get the saved enabled series
     * @return Set of series keys that should be enabled
     */
    const std::unordered_set<std::string>& getSavedEnabledSeries() const;
    
    /**
     * @brief Get the saved group states
     * @return Map of group names to their enabled state
     */
    const std::unordered_map<std::string, bool>& getSavedGroupStates() const;
    
    /**
     * @brief Check if a series should be enabled based on saved state
     * @param series_key The series key to check
     * @return true if the series should be enabled
     */
    bool shouldSeriesBeEnabled(const std::string& series_key) const;
    
    /**
     * @brief Check if a group should be enabled based on saved state
     * @param group_name The group name to check
     * @return true if the group should be enabled, false if disabled, nullopt if no saved state
     */
    std::optional<bool> shouldGroupBeEnabled(const std::string& group_name) const;
    
    /**
     * @brief Clear all saved state
     */
    void clearSavedState();
    
    /**
     * @brief Get debug information about the saved state
     * @return String containing debug information
     */
    std::string getDebugInfo() const;

private:
    std::unordered_set<std::string> _saved_enabled_series;
    std::unordered_map<std::string, bool> _saved_group_enabled_state;
};

#endif // TREEWIDGETSTATEMANAGER_HPP 