#include "MediaWidgetState.hpp"

#include <algorithm>

MediaWidgetState::MediaWidgetState(QObject * parent)
    : EditorState(parent) {
    setDisplayName(QStringLiteral("Media Viewer"));
}

// === Serialization ===

std::string MediaWidgetState::toJson() const {
    return rfl::json::write(_data);
}

bool MediaWidgetState::fromJson(std::string const & json) {
    auto result = rfl::json::read<MediaWidgetStateData>(json);
    if (!result) {
        return false;
    }
    _data = *result;
    emit stateChanged();
    return true;
}

// === Feature Management ===

void MediaWidgetState::setFeatureEnabled(std::string const & key, bool enabled) {
    auto & config = getOrCreateConfig(key);
    if (config.enabled != enabled) {
        config.enabled = enabled;
        markDirty();
        emit featureEnabledChanged(QString::fromStdString(key), enabled);
    }
}

bool MediaWidgetState::isFeatureEnabled(std::string const & key) const {
    auto it = _data.features.find(key);
    if (it != _data.features.end()) {
        return it->second.enabled;
    }
    return false;
}

std::vector<std::string> MediaWidgetState::enabledFeatures() const {
    std::vector<std::string> result;
    for (auto const & [key, config] : _data.features) {
        if (config.enabled) {
            result.push_back(key);
        }
    }
    return result;
}

void MediaWidgetState::setFeatureColor(std::string const & key, std::string const & hex_color) {
    auto & config = getOrCreateConfig(key);
    if (config.color != hex_color) {
        config.color = hex_color;
        markDirty();
        emit featureColorChanged(QString::fromStdString(key), QString::fromStdString(hex_color));
    }
}

std::string MediaWidgetState::getFeatureColor(std::string const & key) const {
    auto it = _data.features.find(key);
    if (it != _data.features.end()) {
        return it->second.color;
    }
    return "";
}

void MediaWidgetState::setFeatureOpacity(std::string const & key, float opacity) {
    opacity = std::clamp(opacity, 0.0f, 1.0f);
    auto & config = getOrCreateConfig(key);
    if (config.opacity != opacity) {
        config.opacity = opacity;
        markDirty();
        emit featureOpacityChanged(QString::fromStdString(key), opacity);
    }
}

float MediaWidgetState::getFeatureOpacity(std::string const & key) const {
    auto it = _data.features.find(key);
    if (it != _data.features.end()) {
        return it->second.opacity;
    }
    return 1.0f;
}

void MediaWidgetState::removeFeature(std::string const & key) {
    auto it = _data.features.find(key);
    if (it != _data.features.end()) {
        _data.features.erase(it);
        markDirty();
        emit featureRemoved(QString::fromStdString(key));
    }
}

// === Viewport ===

void MediaWidgetState::setZoom(double zoom) {
    zoom = std::max(0.01, zoom);// Prevent zero/negative zoom
    if (_data.zoom_level != zoom) {
        _data.zoom_level = zoom;
        _data.user_zoom_active = true;
        markDirty();
        emit zoomChanged(zoom);
    }
}

void MediaWidgetState::setPan(double x, double y) {
    if (_data.pan_x != x || _data.pan_y != y) {
        _data.pan_x = x;
        _data.pan_y = y;
        markDirty();
        emit panChanged(x, y);
    }
}

void MediaWidgetState::setUserZoomActive(bool active) {
    if (_data.user_zoom_active != active) {
        _data.user_zoom_active = active;
        markDirty();
    }
}

void MediaWidgetState::resetViewport() {
    _data.zoom_level = 1.0;
    _data.pan_x = 0.0;
    _data.pan_y = 0.0;
    _data.user_zoom_active = false;
    markDirty();
    emit viewportReset();
}

// === Colormap ===

void MediaWidgetState::setColormapName(std::string const & name) {
    if (_data.colormap_name != name) {
        _data.colormap_name = name;
        markDirty();
        emit colormapChanged();
    }
}

void MediaWidgetState::setColormapRange(double min, double max) {
    if (_data.colormap_min != min || _data.colormap_max != max) {
        _data.colormap_min = min;
        _data.colormap_max = max;
        markDirty();
        emit colormapChanged();
    }
}

void MediaWidgetState::setColormapAutoRange(bool auto_range) {
    if (_data.colormap_auto_range != auto_range) {
        _data.colormap_auto_range = auto_range;
        markDirty();
        emit colormapChanged();
    }
}

// === Text Overlay ===

void MediaWidgetState::setShowFrameNumber(bool show) {
    if (_data.show_frame_number != show) {
        _data.show_frame_number = show;
        markDirty();
        emit textOverlayChanged();
    }
}

void MediaWidgetState::setShowTimestamp(bool show) {
    if (_data.show_timestamp != show) {
        _data.show_timestamp = show;
        markDirty();
        emit textOverlayChanged();
    }
}

// === Private Helpers ===

MediaFeatureConfig & MediaWidgetState::getOrCreateConfig(std::string const & key) {
    auto it = _data.features.find(key);
    if (it == _data.features.end()) {
        _data.features[key] = MediaFeatureConfig{};
        return _data.features[key];
    }
    return it->second;
}
