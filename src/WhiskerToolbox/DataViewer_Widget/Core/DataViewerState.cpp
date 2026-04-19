#include "DataViewerState.hpp"

#include "Plots/Common/MultiLaneVerticalAxisWidget/Core/MultiLaneVerticalAxisState.hpp"

#include <cmath>

DataViewerState::DataViewerState(QObject * parent)
    : EditorState(parent) {
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();

    // Connect registry signals to state signals for forwarding
    _connectRegistrySignals();
}

DataViewerState::~DataViewerState() = default;

void DataViewerState::_connectRegistrySignals() {
    // Forward registry signals to state signals
    connect(&_series_options, &SeriesOptionsRegistry::optionsChanged,
            this, [this](QString const & key, QString const & type) {
                markDirty();
                emit seriesOptionsChanged(key, type);
                emit stateChanged();
            });

    connect(&_series_options, &SeriesOptionsRegistry::optionsRemoved,
            this, [this](QString const & key, QString const & type) {
                markDirty();
                emit seriesOptionsRemoved(key, type);
                emit stateChanged();
            });

    connect(&_series_options, &SeriesOptionsRegistry::visibilityChanged,
            this, [this](QString const & key, QString const & type, bool visible) {
                markDirty();
                emit seriesVisibilityChanged(key, type, visible);
                emit stateChanged();
            });
}

// ==================== Type Identification ====================

QString DataViewerState::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void DataViewerState::setDisplayName(QString const & name) {
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

// ==================== Serialization ====================

std::string DataViewerState::toJson() const {
    // Include instance_id in serialization for restoration
    DataViewerStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool DataViewerState::fromJson(std::string const & json) {
    auto result = rfl::json::read<DataViewerStateData>(json);
    if (result) {
        _data = *result;

        // Restore instance ID from serialized data
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }

        emit stateChanged();
        emit viewStateChanged();
        emit themeChanged();
        emit gridChanged();
        emit uiPreferencesChanged();
        emit layoutConfigChanged();
        emit interactionModeChanged(_data.interaction.mode);
        return true;
    }
    return false;
}

// ==================== View State ====================

void DataViewerState::setTimeWindow(int64_t start, int64_t end) {
    auto const current_start = static_cast<int64_t>(_data.view.x_min);
    auto const current_end = static_cast<int64_t>(_data.view.x_max);
    if (current_start != start || current_end != end) {
        _data.view.x_min = static_cast<double>(start);
        _data.view.x_max = static_cast<double>(end);
        markDirty();
        emit timeWindowChanged();
        emit viewStateChanged();
    }
}

std::pair<int64_t, int64_t> DataViewerState::timeWindow() const {
    return {static_cast<int64_t>(_data.view.x_min), static_cast<int64_t>(_data.view.x_max)};
}

void DataViewerState::adjustTimeWidth(int64_t delta) {
    int64_t const center = timeCenter();
    int64_t new_width = timeWidth() + delta;
    if (new_width < 1) {
        new_width = 1;
    }
    int64_t const half_width = new_width / 2;
    int64_t const new_start = center - half_width;
    int64_t const new_end = new_start + new_width - 1;
    setTimeWindow(new_start, new_end);
}

int64_t DataViewerState::setTimeWidth(int64_t width) {
    if (width < 1) {
        width = 1;
    }
    int64_t const center = timeCenter();
    int64_t const half_width = width / 2;
    int64_t const new_start = center - half_width;
    int64_t const new_end = new_start + width - 1;
    setTimeWindow(new_start, new_end);
    return timeWidth();
}

void DataViewerState::setYBounds(float y_min, float y_max) {
    constexpr float epsilon = 1e-6f;
    if (std::abs(static_cast<float>(_data.view.y_min) - y_min) > epsilon ||
        std::abs(static_cast<float>(_data.view.y_max) - y_max) > epsilon) {
        _data.view.y_min = static_cast<double>(y_min);
        _data.view.y_max = static_cast<double>(y_max);
        markDirty();
        emit viewStateChanged();
    }
}

std::pair<float, float> DataViewerState::yBounds() const {
    return {static_cast<float>(_data.view.y_min), static_cast<float>(_data.view.y_max)};
}

void DataViewerState::setVerticalPanOffset(float offset) {
    constexpr float epsilon = 1e-6f;
    if (std::abs(static_cast<float>(_data.view.y_pan) - offset) > epsilon) {
        _data.view.y_pan = static_cast<double>(offset);
        markDirty();
        emit viewStateChanged();
    }
}

void DataViewerState::setGlobalYScale(float scale) {
    constexpr float epsilon = 1e-6f;
    if (std::abs(_data.global_y_scale - scale) > epsilon) {
        _data.global_y_scale = scale;
        markDirty();
        emit viewStateChanged();
    }
}

void DataViewerState::setViewState(CorePlotting::ViewStateData const & view) {
    constexpr float epsilon = 1e-6f;
    auto const old_start = static_cast<int64_t>(_data.view.x_min);
    auto const old_end = static_cast<int64_t>(_data.view.x_max);
    auto const new_start = static_cast<int64_t>(view.x_min);
    auto const new_end = static_cast<int64_t>(view.x_max);
    bool const time_changed = old_start != new_start || old_end != new_end;
    bool const y_changed = std::abs(static_cast<float>(_data.view.y_min) - static_cast<float>(view.y_min)) > epsilon ||
                           std::abs(static_cast<float>(_data.view.y_max) - static_cast<float>(view.y_max)) > epsilon;
    bool const pan_changed = std::abs(static_cast<float>(_data.view.y_pan) - static_cast<float>(view.y_pan)) > epsilon;
    bool const zoom_changed = std::abs(static_cast<float>(_data.view.y_zoom) - static_cast<float>(view.y_zoom)) > epsilon;

    if (time_changed || y_changed || pan_changed || zoom_changed) {
        _data.view = view;
        markDirty();
        if (time_changed) {
            emit timeWindowChanged();
        }
        emit viewStateChanged();
    }
}

// ==================== Theme ====================

void DataViewerState::setTheme(DataViewerTheme theme) {
    if (_data.theme.theme != theme) {
        _data.theme.theme = theme;
        markDirty();
        emit themeChanged();
    }
}

void DataViewerState::setBackgroundColor(QString const & hex) {
    std::string const hex_std = hex.toStdString();
    if (_data.theme.background_color != hex_std) {
        _data.theme.background_color = hex_std;
        markDirty();
        emit themeChanged();
    }
}

void DataViewerState::setAxisColor(QString const & hex) {
    std::string const hex_std = hex.toStdString();
    if (_data.theme.axis_color != hex_std) {
        _data.theme.axis_color = hex_std;
        markDirty();
        emit themeChanged();
    }
}

void DataViewerState::setThemeState(DataViewerThemeState const & theme_state) {
    if (_data.theme.theme != theme_state.theme ||
        _data.theme.background_color != theme_state.background_color ||
        _data.theme.axis_color != theme_state.axis_color) {
        _data.theme = theme_state;
        markDirty();
        emit themeChanged();
    }
}

// ==================== Grid ====================

void DataViewerState::setGridEnabled(bool enabled) {
    if (_data.grid.enabled != enabled) {
        _data.grid.enabled = enabled;
        markDirty();
        emit gridChanged();
    }
}

void DataViewerState::setGridSpacing(int spacing) {
    if (_data.grid.spacing != spacing) {
        _data.grid.spacing = spacing;
        markDirty();
        emit gridChanged();
    }
}

void DataViewerState::setGridState(DataViewerGridState const & grid_state) {
    if (_data.grid.enabled != grid_state.enabled ||
        _data.grid.spacing != grid_state.spacing) {
        _data.grid = grid_state;
        markDirty();
        emit gridChanged();
    }
}

// ==================== UI Preferences ====================

void DataViewerState::setZoomScalingMode(DataViewerZoomScalingMode mode) {
    if (_data.ui.zoom_scaling_mode != mode) {
        _data.ui.zoom_scaling_mode = mode;
        markDirty();
        emit uiPreferencesChanged();
    }
}

void DataViewerState::setPropertiesPanelCollapsed(bool collapsed) {
    if (_data.ui.properties_panel_collapsed != collapsed) {
        _data.ui.properties_panel_collapsed = collapsed;
        markDirty();
        emit uiPreferencesChanged();
    }
}

void DataViewerState::setUIPreferences(DataViewerUIPreferences const & prefs) {
    if (_data.ui.zoom_scaling_mode != prefs.zoom_scaling_mode ||
        _data.ui.properties_panel_collapsed != prefs.properties_panel_collapsed ||
        _data.ui.developer_mode != prefs.developer_mode) {
        bool const dev_mode_changed = _data.ui.developer_mode != prefs.developer_mode;
        _data.ui = prefs;
        markDirty();
        emit uiPreferencesChanged();
        if (dev_mode_changed) {
            emit developerModeChanged(_data.ui.developer_mode);
        }
    }
}

// ==================== Developer Mode ====================

void DataViewerState::setDeveloperMode(bool enabled) {
    if (_data.ui.developer_mode != enabled) {
        _data.ui.developer_mode = enabled;
        markDirty();
        emit developerModeChanged(enabled);
    }
}

// ==================== Layout Config ====================

void DataViewerState::setLaneSizingPolicy(CorePlotting::LaneSizingPolicy policy) {
    if (_data.layout.lane_sizing_policy != policy) {
        _data.layout.lane_sizing_policy = policy;
        // Reset vertical pan so the view centers on the data in the new layout mode
        _data.view.y_pan = 0.0;
        markDirty();
        emit layoutConfigChanged();
        emit viewStateChanged();
    }
}

void DataViewerState::setLaneHeight(float height) {
    constexpr float epsilon = 1e-6f;
    if (std::abs(_data.layout.lane_height - height) > epsilon) {
        _data.layout.lane_height = height;
        markDirty();
        emit layoutConfigChanged();
        emit viewStateChanged();
    }
}

void DataViewerState::setLaneGap(float gap) {
    constexpr float epsilon = 1e-6f;
    if (std::abs(_data.layout.lane_gap - gap) > epsilon) {
        _data.layout.lane_gap = gap;
        markDirty();
        emit layoutConfigChanged();
        emit viewStateChanged();
    }
}

void DataViewerState::setLayoutConfig(DataViewerLayoutConfig const & config) {
    constexpr float epsilon = 1e-6f;
    bool const changed = _data.layout.lane_sizing_policy != config.lane_sizing_policy ||
                         std::abs(_data.layout.lane_height - config.lane_height) > epsilon ||
                         std::abs(_data.layout.lane_gap - config.lane_gap) > epsilon;
    if (changed) {
        _data.layout = config;
        markDirty();
        emit layoutConfigChanged();
        emit viewStateChanged();
    }
}

// ==================== Interaction ====================

void DataViewerState::setInteractionMode(DataViewerInteractionMode mode) {
    if (_data.interaction.mode != mode) {
        _data.interaction.mode = mode;
        markDirty();
        emit interactionModeChanged(mode);
    }
}

// ==================== Group Scaling ====================

void DataViewerState::setGroupScaling(std::string const & group_name, GroupScalingState const & state) {
    _data.group_scaling[group_name] = state;
    markDirty();
    emit groupScalingChanged(QString::fromStdString(group_name));
    emit stateChanged();
}

GroupScalingState const * DataViewerState::getGroupScaling(std::string const & group_name) const {
    auto it = _data.group_scaling.find(group_name);
    if (it != _data.group_scaling.end()) {
        return &it->second;
    }
    return nullptr;
}

GroupScalingState * DataViewerState::getGroupScalingMutable(std::string const & group_name) {
    auto it = _data.group_scaling.find(group_name);
    if (it != _data.group_scaling.end()) {
        return &it->second;
    }
    return nullptr;
}

bool DataViewerState::isGroupUnifiedScaling(std::string const & group_name) const {
    auto const * gs = getGroupScaling(group_name);
    // Default to true if no state exists yet
    return gs == nullptr || gs->unified_scaling;
}

void DataViewerState::removeGroupScaling(std::string const & group_name) {
    auto it = _data.group_scaling.find(group_name);
    if (it != _data.group_scaling.end()) {
        _data.group_scaling.erase(it);
        markDirty();
        emit groupScalingChanged(QString::fromStdString(group_name));
        emit stateChanged();
    }
}

// ==================== Multi-Lane Axis ====================

MultiLaneVerticalAxisState * DataViewerState::multiLaneAxisState() {
    if (!_multi_lane_axis_state) {
        _multi_lane_axis_state = std::make_unique<MultiLaneVerticalAxisState>(this);
    }
    return _multi_lane_axis_state.get();
}
