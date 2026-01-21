#include "TestWidgetState.hpp"

#include <algorithm>

TestWidgetState::TestWidgetState(std::shared_ptr<DataManager> data_manager,
                                 QObject * parent)
    : EditorState(parent)
    , _data_manager(std::move(data_manager)) {
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();
}

QString TestWidgetState::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void TestWidgetState::setDisplayName(QString const & name) {
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string TestWidgetState::toJson() const {
    // Include instance_id in serialization for restoration
    TestWidgetStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool TestWidgetState::fromJson(std::string const & json) {
    auto result = rfl::json::read<TestWidgetStateData>(json);
    if (result) {
        _data = *result;

        // Restore instance ID from serialized data
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }

        // Emit all signals to update any connected views
        emit stateChanged();
        emit showGridChanged(_data.show_grid);
        emit showCrosshairChanged(_data.show_crosshair);
        emit enableAnimationChanged(_data.enable_animation);
        emit highlightColorChanged(highlightColor());
        emit zoomLevelChanged(_data.zoom_level);
        emit gridSpacingChanged(_data.grid_spacing);
        emit labelTextChanged(QString::fromStdString(_data.label_text));
        
        return true;
    }
    return false;
}

// === Feature Toggles ===

void TestWidgetState::setShowGrid(bool show) {
    if (_data.show_grid != show) {
        _data.show_grid = show;
        markDirty();
        emit showGridChanged(show);
    }
}

void TestWidgetState::setShowCrosshair(bool show) {
    if (_data.show_crosshair != show) {
        _data.show_crosshair = show;
        markDirty();
        emit showCrosshairChanged(show);
    }
}

void TestWidgetState::setEnableAnimation(bool enable) {
    if (_data.enable_animation != enable) {
        _data.enable_animation = enable;
        markDirty();
        emit enableAnimationChanged(enable);
    }
}

// === Color ===

QColor TestWidgetState::highlightColor() const {
    return QColor(QString::fromStdString(_data.highlight_color));
}

QString TestWidgetState::highlightColorHex() const {
    return QString::fromStdString(_data.highlight_color);
}

void TestWidgetState::setHighlightColor(QColor const & color) {
    setHighlightColor(color.name());
}

void TestWidgetState::setHighlightColor(QString const & hexColor) {
    std::string const hex_std = hexColor.toStdString();
    if (_data.highlight_color != hex_std) {
        _data.highlight_color = hex_std;
        markDirty();
        emit highlightColorChanged(QColor(hexColor));
    }
}

// === Numeric Values ===

void TestWidgetState::setZoomLevel(double zoom) {
    // Clamp to valid range
    zoom = std::clamp(zoom, 0.1, 5.0);
    
    if (_data.zoom_level != zoom) {
        _data.zoom_level = zoom;
        markDirty();
        emit zoomLevelChanged(zoom);
    }
}

void TestWidgetState::setGridSpacing(int spacing) {
    // Clamp to valid range
    spacing = std::clamp(spacing, 10, 200);
    
    if (_data.grid_spacing != spacing) {
        _data.grid_spacing = spacing;
        markDirty();
        emit gridSpacingChanged(spacing);
    }
}

// === Text ===

QString TestWidgetState::labelText() const {
    return QString::fromStdString(_data.label_text);
}

void TestWidgetState::setLabelText(QString const & text) {
    std::string const text_std = text.toStdString();
    if (_data.label_text != text_std) {
        _data.label_text = text_std;
        markDirty();
        emit labelTextChanged(text);
    }
}
