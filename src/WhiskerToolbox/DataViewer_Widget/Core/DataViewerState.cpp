#include "DataViewerState.hpp"

#include "Plots/Common/MultiLaneVerticalAxisWidget/Core/MultiLaneVerticalAxisState.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace {

[[nodiscard]] bool isNearlyEqual(float a, float b, float epsilon = 1e-6F) {
    return std::abs(a - b) <= epsilon;
}

[[nodiscard]] SeriesLaneOverrideData normalizeSeriesLaneOverride(SeriesLaneOverrideData value) {
    if (!std::isfinite(value.lane_weight) || value.lane_weight <= 0.0F) {
        value.lane_weight = 1.0F;
    }

    if (value.lane_id.empty()) {
        value.lane_order = std::nullopt;
        value.lane_weight = 1.0F;
        value.overlay_mode = LaneOverlayMode::Auto;
        value.overlay_z = 0;
    }

    return value;
}

[[nodiscard]] bool isDefaultSeriesLaneOverride(SeriesLaneOverrideData const & value) {
    return value.lane_id.empty() &&
           !value.lane_order.has_value() &&
           isNearlyEqual(value.lane_weight, 1.0F) &&
           value.overlay_mode == LaneOverlayMode::Auto &&
           value.overlay_z == 0;
}

[[nodiscard]] LaneOverrideData normalizeLaneOverride(LaneOverrideData value) {
    if (value.lane_weight.has_value()) {
        float const candidate = *value.lane_weight;
        if (!std::isfinite(candidate) || candidate <= 0.0F) {
            value.lane_weight = std::nullopt;
        }
    }
    return value;
}

[[nodiscard]] bool isDefaultLaneOverride(LaneOverrideData const & value) {
    return value.display_label.empty() && !value.lane_weight.has_value();
}

void normalizeAllLaneOverrides(DataViewerStateData & data) {
    std::map<std::string, SeriesLaneOverrideData> normalized_series;
    for (auto const & [series_key, override_data]: data.series_lane_overrides) {
        auto normalized = normalizeSeriesLaneOverride(override_data);
        if (!isDefaultSeriesLaneOverride(normalized)) {
            normalized_series[series_key] = normalized;
        }
    }

    // Resolve conflicting lane_order values deterministically by series key ordering.
    std::set<int> used_lane_orders;
    for (auto & [_, override_data]: normalized_series) {
        if (!override_data.lane_order.has_value()) {
            continue;
        }
        int lane_order = *override_data.lane_order;
        while (used_lane_orders.contains(lane_order)) {
            ++lane_order;
        }
        override_data.lane_order = lane_order;
        used_lane_orders.insert(lane_order);
    }
    data.series_lane_overrides = std::move(normalized_series);

    std::map<std::string, LaneOverrideData> normalized_lanes;
    for (auto const & [lane_id, override_data]: data.lane_overrides) {
        if (lane_id.empty()) {
            continue;
        }
        auto normalized = normalizeLaneOverride(override_data);
        if (!isDefaultLaneOverride(normalized)) {
            normalized_lanes[lane_id] = normalized;
        }
    }
    data.lane_overrides = std::move(normalized_lanes);
}

}// namespace

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
    normalizeAllLaneOverrides(data_to_serialize);
    return rfl::json::write(data_to_serialize);
}

bool DataViewerState::fromJson(std::string const & json) {
    auto result = rfl::json::read<DataViewerStateData>(json);
    if (result) {
        _data = *result;
        normalizeAllLaneOverrides(_data);

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

void DataViewerState::setMarginFactor(float factor) {
    constexpr float epsilon = 1e-6f;
    if (std::abs(_data.layout.margin_factor - factor) > epsilon) {
        _data.layout.margin_factor = factor;
        markDirty();
        emit layoutConfigChanged();
    }
}

void DataViewerState::setLayoutConfig(DataViewerLayoutConfig const & config) {
    constexpr float epsilon = 1e-6f;
    bool const changed = std::abs(_data.layout.margin_factor - config.margin_factor) > epsilon;
    if (changed) {
        _data.layout = config;
        markDirty();
        emit layoutConfigChanged();
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

// ==================== Mixed-Lane Overrides ====================

void DataViewerState::setSeriesLaneOverride(std::string const & series_key, SeriesLaneOverrideData const & override_data) {
    auto normalized = normalizeSeriesLaneOverride(override_data);

    bool changed = false;
    if (isDefaultSeriesLaneOverride(normalized)) {
        auto it = _data.series_lane_overrides.find(series_key);
        if (it != _data.series_lane_overrides.end()) {
            _data.series_lane_overrides.erase(it);
            changed = true;
        }
    } else {
        auto it = _data.series_lane_overrides.find(series_key);
        if (it == _data.series_lane_overrides.end() || it->second.lane_id != normalized.lane_id ||
            it->second.lane_order != normalized.lane_order ||
            !isNearlyEqual(it->second.lane_weight, normalized.lane_weight) ||
            it->second.overlay_mode != normalized.overlay_mode ||
            it->second.overlay_z != normalized.overlay_z) {
            _data.series_lane_overrides[series_key] = normalized;
            changed = true;
        }
    }

    if (changed) {
        normalizeAllLaneOverrides(_data);
        markDirty();
        emit seriesLaneOverrideChanged(QString::fromStdString(series_key));
        emit stateChanged();
    }
}

void DataViewerState::clearSeriesLaneOverride(std::string const & series_key) {
    auto it = _data.series_lane_overrides.find(series_key);
    if (it != _data.series_lane_overrides.end()) {
        _data.series_lane_overrides.erase(it);
        markDirty();
        emit seriesLaneOverrideChanged(QString::fromStdString(series_key));
        emit stateChanged();
    }
}

SeriesLaneOverrideData const * DataViewerState::getSeriesLaneOverride(std::string const & series_key) const {
    auto it = _data.series_lane_overrides.find(series_key);
    if (it != _data.series_lane_overrides.end()) {
        return &it->second;
    }
    return nullptr;
}

void DataViewerState::setLaneOverride(std::string const & lane_id, LaneOverrideData const & override_data) {
    if (lane_id.empty()) {
        return;
    }

    auto normalized = normalizeLaneOverride(override_data);
    bool changed = false;
    if (isDefaultLaneOverride(normalized)) {
        auto it = _data.lane_overrides.find(lane_id);
        if (it != _data.lane_overrides.end()) {
            _data.lane_overrides.erase(it);
            changed = true;
        }
    } else {
        auto it = _data.lane_overrides.find(lane_id);
        if (it == _data.lane_overrides.end() ||
            it->second.display_label != normalized.display_label ||
            it->second.lane_weight != normalized.lane_weight) {
            _data.lane_overrides[lane_id] = normalized;
            changed = true;
        }
    }

    if (changed) {
        markDirty();
        emit laneOverrideChanged(QString::fromStdString(lane_id));
        emit stateChanged();
    }
}

void DataViewerState::clearLaneOverride(std::string const & lane_id) {
    auto it = _data.lane_overrides.find(lane_id);
    if (it != _data.lane_overrides.end()) {
        _data.lane_overrides.erase(it);
        markDirty();
        emit laneOverrideChanged(QString::fromStdString(lane_id));
        emit stateChanged();
    }
}

LaneOverrideData const * DataViewerState::getLaneOverride(std::string const & lane_id) const {
    auto it = _data.lane_overrides.find(lane_id);
    if (it != _data.lane_overrides.end()) {
        return &it->second;
    }
    return nullptr;
}

// ==================== Multi-Lane Axis ====================

MultiLaneVerticalAxisState * DataViewerState::multiLaneAxisState() {
    if (!_multi_lane_axis_state) {
        _multi_lane_axis_state = std::make_unique<MultiLaneVerticalAxisState>(this);
    }
    return _multi_lane_axis_state.get();
}
