#include "ScatterPlotState.hpp"

#include "Plots/Common/GlyphStyleWidget/Core/GlyphStyleState.hpp"
#include "Plots/Common/HorizontalAxisWidget/Core/HorizontalAxisState.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisState.hpp"

#include <rfl/json.hpp>

#include <algorithm>

ScatterPlotState::ScatterPlotState(QObject * parent)
    : EditorState(parent),
      _horizontal_axis_state(std::make_unique<HorizontalAxisState>(this)),
      _vertical_axis_state(std::make_unique<VerticalAxisState>(this)),
      _glyph_style_state(std::make_unique<GlyphStyleState>(this))
{
    _data.instance_id = getInstanceId().toStdString();
    _data.horizontal_axis = _horizontal_axis_state->data();
    _data.vertical_axis = _vertical_axis_state->data();

    // Sync view state bounds from axis states so view state and axes never drift
    _data.view_state.x_min = _horizontal_axis_state->getXMin();
    _data.view_state.x_max = _horizontal_axis_state->getXMax();
    _data.view_state.y_min = _vertical_axis_state->getYMin();
    _data.view_state.y_max = _vertical_axis_state->getYMax();

    auto syncHorizontalData = [this]() {
        _data.horizontal_axis = _horizontal_axis_state->data();
        markDirty();
        emit stateChanged();
    };
    connect(_horizontal_axis_state.get(), &HorizontalAxisState::rangeChanged,
            this, syncHorizontalData);
    connect(_horizontal_axis_state.get(), &HorizontalAxisState::rangeUpdated,
            this, syncHorizontalData);

    auto syncVerticalData = [this]() {
        _data.vertical_axis = _vertical_axis_state->data();
        markDirty();
        emit stateChanged();
    };
    connect(_vertical_axis_state.get(), &VerticalAxisState::rangeChanged,
            this, syncVerticalData);
    connect(_vertical_axis_state.get(), &VerticalAxisState::rangeUpdated,
            this, syncVerticalData);

    // Sync glyph style state with serializable data
    _glyph_style_state->setStyleSilent(_data.glyph_style);
    connect(_glyph_style_state.get(), &GlyphStyleState::styleChanged,
            this, [this]() {
                _data.glyph_style = _glyph_style_state->data();
                markDirty();
                emit glyphStyleChanged();
                emit stateChanged();
            });
}

QString ScatterPlotState::getDisplayName() const
{
    return QString::fromStdString(_data.display_name);
}

void ScatterPlotState::setDisplayName(QString const & name)
{
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

double ScatterPlotState::getXMin() const
{
    return _horizontal_axis_state->getXMin();
}

double ScatterPlotState::getXMax() const
{
    return _horizontal_axis_state->getXMax();
}

double ScatterPlotState::getYMin() const
{
    return _vertical_axis_state->getYMin();
}

double ScatterPlotState::getYMax() const
{
    return _vertical_axis_state->getYMax();
}

void ScatterPlotState::setXZoom(double zoom)
{
    if (_data.view_state.x_zoom != zoom) {
        _data.view_state.x_zoom = zoom;
        markDirty();
        emit viewStateChanged();
    }
}

void ScatterPlotState::setYZoom(double zoom)
{
    if (_data.view_state.y_zoom != zoom) {
        _data.view_state.y_zoom = zoom;
        markDirty();
        emit viewStateChanged();
    }
}

void ScatterPlotState::setPan(double x_pan, double y_pan)
{
    if (_data.view_state.x_pan != x_pan || _data.view_state.y_pan != y_pan) {
        _data.view_state.x_pan = x_pan;
        _data.view_state.y_pan = y_pan;
        markDirty();
        emit viewStateChanged();
    }
}

void ScatterPlotState::setXBounds(double x_min, double x_max)
{
    if (_data.view_state.x_min != x_min || _data.view_state.x_max != x_max) {
        _data.view_state.x_min = x_min;
        _data.view_state.x_max = x_max;
        _horizontal_axis_state->data().x_min = x_min;
        _horizontal_axis_state->data().x_max = x_max;
        _data.horizontal_axis = _horizontal_axis_state->data();
        markDirty();
        emit viewStateChanged();
        emit stateChanged();
    }
}

void ScatterPlotState::setYBounds(double y_min, double y_max)
{
    if (_data.view_state.y_min != y_min || _data.view_state.y_max != y_max) {
        _data.view_state.y_min = y_min;
        _data.view_state.y_max = y_max;
        _vertical_axis_state->data().y_min = y_min;
        _vertical_axis_state->data().y_max = y_max;
        _data.vertical_axis = _vertical_axis_state->data();
        markDirty();
        emit viewStateChanged();
        emit stateChanged();
    }
}

void ScatterPlotState::setXSource(std::optional<ScatterAxisSource> source)
{
    _data.x_source = std::move(source);
    markDirty();
    emit xSourceChanged();
    emit stateChanged();
}

void ScatterPlotState::setYSource(std::optional<ScatterAxisSource> source)
{
    _data.y_source = std::move(source);
    markDirty();
    emit ySourceChanged();
    emit stateChanged();
}

void ScatterPlotState::setShowReferenceLine(bool show)
{
    if (_data.show_reference_line != show) {
        _data.show_reference_line = show;
        markDirty();
        emit referenceLineChanged();
        emit stateChanged();
    }
}

void ScatterPlotState::setColorByGroup(bool enabled)
{
    if (_data.color_by_group != enabled) {
        _data.color_by_group = enabled;
        markDirty();
        emit colorByGroupChanged();
        emit stateChanged();
    }
}

ScatterSelectionMode ScatterPlotState::selectionMode() const
{
    if (_data.selection_mode == "polygon") {
        return ScatterSelectionMode::Polygon;
    }
    return ScatterSelectionMode::SinglePoint;
}

void ScatterPlotState::setSelectionMode(ScatterSelectionMode mode)
{
    std::string const mode_str = (mode == ScatterSelectionMode::Polygon) ? "polygon" : "single_point";
    if (_data.selection_mode != mode_str) {
        _data.selection_mode = mode_str;
        // Clear selection when switching modes
        clearSelection();
        markDirty();
        emit selectionModeChanged();
        emit stateChanged();
    }
}

void ScatterPlotState::setSelectedIndices(std::vector<std::size_t> indices)
{
    _data.selected_indices.value() = std::move(indices);
    emit selectionChanged();
}

void ScatterPlotState::addSelectedIndex(std::size_t index)
{
    auto & sel = _data.selected_indices.value();
    if (std::find(sel.begin(), sel.end(), index) == sel.end()) {
        sel.push_back(index);
        emit selectionChanged();
    }
}

void ScatterPlotState::removeSelectedIndex(std::size_t index)
{
    auto & sel = _data.selected_indices.value();
    auto it = std::find(sel.begin(), sel.end(), index);
    if (it != sel.end()) {
        sel.erase(it);
        emit selectionChanged();
    }
}

void ScatterPlotState::clearSelection()
{
    if (!_data.selected_indices.value().empty()) {
        _data.selected_indices.value().clear();
        emit selectionChanged();
    }
}

bool ScatterPlotState::isSelected(std::size_t index) const
{
    auto const & sel = _data.selected_indices.value();
    return std::find(sel.begin(), sel.end(), index) != sel.end();
}

std::string ScatterPlotState::toJson() const
{
    ScatterPlotStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool ScatterPlotState::fromJson(std::string const & json)
{
    auto result = rfl::json::read<ScatterPlotStateData>(json);
    if (result) {
        _data = *result;
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }
        _horizontal_axis_state->data() = _data.horizontal_axis;
        _vertical_axis_state->data() = _data.vertical_axis;
        _glyph_style_state->setStyleSilent(_data.glyph_style);

        // Sync view state bounds from axis states so they never drift
        _data.view_state.x_min = _horizontal_axis_state->getXMin();
        _data.view_state.x_max = _horizontal_axis_state->getXMax();
        _data.view_state.y_min = _vertical_axis_state->getYMin();
        _data.view_state.y_max = _vertical_axis_state->getYMax();

        emit stateChanged();
        return true;
    }
    return false;
}
