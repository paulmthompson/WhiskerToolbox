#include "TreeWidgetStateManager.hpp"

#include <optional>
#include <sstream>

void TreeWidgetStateManager::saveEnabledSeries(const std::unordered_set<std::string>& enabled_series) {
    _saved_enabled_series = enabled_series;
    
    std::cout << "TreeWidgetStateManager: Saved " << enabled_series.size() << " enabled series:" << std::endl;
    for (const auto& series : enabled_series) {
        std::cout << "  - " << series << std::endl;
    }
}

void TreeWidgetStateManager::saveGroupStates(const std::unordered_map<std::string, bool>& group_enabled_state) {
    _saved_group_enabled_state = group_enabled_state;
    
    std::cout << "TreeWidgetStateManager: Saved " << group_enabled_state.size() << " group states:" << std::endl;
    for (const auto& [group, enabled] : group_enabled_state) {
        std::cout << "  - " << group << ": " << (enabled ? "enabled" : "disabled") << std::endl;
    }
}

const std::unordered_set<std::string>& TreeWidgetStateManager::getSavedEnabledSeries() const {
    return _saved_enabled_series;
}

const std::unordered_map<std::string, bool>& TreeWidgetStateManager::getSavedGroupStates() const {
    return _saved_group_enabled_state;
}

bool TreeWidgetStateManager::shouldSeriesBeEnabled(const std::string& series_key) const {
    bool should_enable = _saved_enabled_series.find(series_key) != _saved_enabled_series.end();
    std::cout << "TreeWidgetStateManager: Series '" << series_key << "' should be " 
              << (should_enable ? "enabled" : "disabled") << std::endl;
    return should_enable;
}

std::optional<bool> TreeWidgetStateManager::shouldGroupBeEnabled(const std::string& group_name) const {
    auto it = _saved_group_enabled_state.find(group_name);
    if (it != _saved_group_enabled_state.end()) {
        std::cout << "TreeWidgetStateManager: Group '" << group_name << "' should be " 
                  << (it->second ? "enabled" : "disabled") << std::endl;
        return it->second;
    }
    
    std::cout << "TreeWidgetStateManager: No saved state for group '" << group_name << "'" << std::endl;
    return std::nullopt;
}

void TreeWidgetStateManager::clearSavedState() {
    std::cout << "TreeWidgetStateManager: Clearing saved state" << std::endl;
    _saved_enabled_series.clear();
    _saved_group_enabled_state.clear();
}

std::string TreeWidgetStateManager::getDebugInfo() const {
    std::ostringstream oss;
    oss << "TreeWidgetStateManager State:\n";
    oss << "  Enabled Series (" << _saved_enabled_series.size() << "):\n";
    for (const auto& series : _saved_enabled_series) {
        oss << "    - " << series << "\n";
    }
    oss << "  Group States (" << _saved_group_enabled_state.size() << "):\n";
    for (const auto& [group, enabled] : _saved_group_enabled_state) {
        oss << "    - " << group << ": " << (enabled ? "enabled" : "disabled") << "\n";
    }
    return oss.str();
} 