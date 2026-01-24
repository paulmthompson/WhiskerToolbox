#include "DataViewerState.hpp"

#include <cmath>

DataViewerState::DataViewerState(QObject * parent)
    : EditorState(parent)
{
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();

    // Connect registry signals to state signals for forwarding
    _connectRegistrySignals();
}

void DataViewerState::_connectRegistrySignals()
{
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

QString DataViewerState::getDisplayName() const
{
    return QString::fromStdString(_data.display_name);
}

void DataViewerState::setDisplayName(QString const & name)
{
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

// ==================== Serialization ====================

std::string DataViewerState::toJson() const
{
    // Include instance_id in serialization for restoration
    DataViewerStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool DataViewerState::fromJson(std::string const & json)
{
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
        emit interactionModeChanged(_data.interaction.mode);
        return true;
    }
    return false;
}

// ==================== View State ====================

void DataViewerState::setTimeWindow(int64_t start, int64_t end)
{
    if (_data.view.time_start != start || _data.view.time_end != end) {
        _data.view.time_start = start;
        _data.view.time_end = end;
        markDirty();
        emit viewStateChanged();
    }
}

std::pair<int64_t, int64_t> DataViewerState::timeWindow() const
{
    return {_data.view.time_start, _data.view.time_end};
}

void DataViewerState::setYBounds(float y_min, float y_max)
{
    constexpr float epsilon = 1e-6f;
    if (std::abs(_data.view.y_min - y_min) > epsilon ||
        std::abs(_data.view.y_max - y_max) > epsilon) {
        _data.view.y_min = y_min;
        _data.view.y_max = y_max;
        markDirty();
        emit viewStateChanged();
    }
}

std::pair<float, float> DataViewerState::yBounds() const
{
    return {_data.view.y_min, _data.view.y_max};
}

void DataViewerState::setVerticalPanOffset(float offset)
{
    constexpr float epsilon = 1e-6f;
    if (std::abs(_data.view.vertical_pan_offset - offset) > epsilon) {
        _data.view.vertical_pan_offset = offset;
        markDirty();
        emit viewStateChanged();
    }
}

void DataViewerState::setGlobalZoom(float zoom)
{
    constexpr float epsilon = 1e-6f;
    if (std::abs(_data.view.global_zoom - zoom) > epsilon) {
        _data.view.global_zoom = zoom;
        markDirty();
        emit viewStateChanged();
    }
}

void DataViewerState::setGlobalVerticalScale(float scale)
{
    constexpr float epsilon = 1e-6f;
    if (std::abs(_data.view.global_vertical_scale - scale) > epsilon) {
        _data.view.global_vertical_scale = scale;
        markDirty();
        emit viewStateChanged();
    }
}

void DataViewerState::setViewState(CorePlotting::TimeSeriesViewState const & view)
{
    constexpr float epsilon = 1e-6f;
    bool const time_changed = _data.view.time_start != view.time_start ||
                              _data.view.time_end != view.time_end;
    bool const y_changed = std::abs(_data.view.y_min - view.y_min) > epsilon ||
                           std::abs(_data.view.y_max - view.y_max) > epsilon;
    bool const pan_changed = std::abs(_data.view.vertical_pan_offset - view.vertical_pan_offset) > epsilon;
    bool const zoom_changed = std::abs(_data.view.global_zoom - view.global_zoom) > epsilon;
    bool const scale_changed = std::abs(_data.view.global_vertical_scale - view.global_vertical_scale) > epsilon;

    if (time_changed || y_changed || pan_changed || zoom_changed || scale_changed) {
        _data.view = view;
        markDirty();
        emit viewStateChanged();
    }
}

// ==================== Theme ====================

void DataViewerState::setTheme(DataViewerTheme theme)
{
    if (_data.theme.theme != theme) {
        _data.theme.theme = theme;
        markDirty();
        emit themeChanged();
    }
}

void DataViewerState::setBackgroundColor(QString const & hex)
{
    std::string const hex_std = hex.toStdString();
    if (_data.theme.background_color != hex_std) {
        _data.theme.background_color = hex_std;
        markDirty();
        emit themeChanged();
    }
}

void DataViewerState::setAxisColor(QString const & hex)
{
    std::string const hex_std = hex.toStdString();
    if (_data.theme.axis_color != hex_std) {
        _data.theme.axis_color = hex_std;
        markDirty();
        emit themeChanged();
    }
}

void DataViewerState::setThemeState(DataViewerThemeState const & theme_state)
{
    if (_data.theme.theme != theme_state.theme ||
        _data.theme.background_color != theme_state.background_color ||
        _data.theme.axis_color != theme_state.axis_color) {
        _data.theme = theme_state;
        markDirty();
        emit themeChanged();
    }
}

// ==================== Grid ====================

void DataViewerState::setGridEnabled(bool enabled)
{
    if (_data.grid.enabled != enabled) {
        _data.grid.enabled = enabled;
        markDirty();
        emit gridChanged();
    }
}

void DataViewerState::setGridSpacing(int spacing)
{
    if (_data.grid.spacing != spacing) {
        _data.grid.spacing = spacing;
        markDirty();
        emit gridChanged();
    }
}

void DataViewerState::setGridState(DataViewerGridState const & grid_state)
{
    if (_data.grid.enabled != grid_state.enabled ||
        _data.grid.spacing != grid_state.spacing) {
        _data.grid = grid_state;
        markDirty();
        emit gridChanged();
    }
}

// ==================== UI Preferences ====================

void DataViewerState::setZoomScalingMode(DataViewerZoomScalingMode mode)
{
    if (_data.ui.zoom_scaling_mode != mode) {
        _data.ui.zoom_scaling_mode = mode;
        markDirty();
        emit uiPreferencesChanged();
    }
}

void DataViewerState::setPropertiesPanelCollapsed(bool collapsed)
{
    if (_data.ui.properties_panel_collapsed != collapsed) {
        _data.ui.properties_panel_collapsed = collapsed;
        markDirty();
        emit uiPreferencesChanged();
    }
}

void DataViewerState::setUIPreferences(DataViewerUIPreferences const & prefs)
{
    if (_data.ui.zoom_scaling_mode != prefs.zoom_scaling_mode ||
        _data.ui.properties_panel_collapsed != prefs.properties_panel_collapsed) {
        _data.ui = prefs;
        markDirty();
        emit uiPreferencesChanged();
    }
}

// ==================== Interaction ====================

void DataViewerState::setInteractionMode(DataViewerInteractionMode mode)
{
    if (_data.interaction.mode != mode) {
        _data.interaction.mode = mode;
        markDirty();
        emit interactionModeChanged(mode);
    }
}
