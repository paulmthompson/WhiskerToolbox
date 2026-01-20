#include "MediaWidgetState.hpp"

#include <algorithm>
#include <cmath>
#include <type_traits>

MediaWidgetState::MediaWidgetState(QObject * parent)
    : EditorState(parent) {
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();
    
    // Connect registry signals to state signals for backward compatibility
    _connectRegistrySignals();
}

void MediaWidgetState::_connectRegistrySignals() {
    // Forward registry signals to state signals
    connect(&_display_options, &DisplayOptionsRegistry::optionsChanged,
            this, [this](QString const & key, QString const & type) {
        markDirty();
        emit displayOptionsChanged(key, type);
        emit stateChanged();
    });
    
    connect(&_display_options, &DisplayOptionsRegistry::optionsRemoved,
            this, [this](QString const & key, QString const & type) {
        markDirty();
        emit displayOptionsRemoved(key, type);
        emit stateChanged();
    });
    
    connect(&_display_options, &DisplayOptionsRegistry::visibilityChanged,
            this, [this](QString const & key, QString const & type, bool visible) {
        markDirty();
        emit featureEnabledChanged(key, type, visible);
        emit stateChanged();
    });
}

// === Type Identification ===

QString MediaWidgetState::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void MediaWidgetState::setDisplayName(QString const & name) {
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

// === Serialization ===

std::string MediaWidgetState::toJson() const {
    // Include instance_id in serialization for restoration
    MediaWidgetStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool MediaWidgetState::fromJson(std::string const & json) {
    auto result = rfl::json::read<MediaWidgetStateData>(json);
    if (result) {
        _data = *result;

        // Restore instance ID from serialized data
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }

        emit stateChanged();
        emit displayedDataKeyChanged(QString::fromStdString(_data.displayed_media_key));
        emit viewportChanged();
        return true;
    }
    return false;
}

// === Displayed Data Key ===

void MediaWidgetState::setDisplayedDataKey(QString const & key) {
    std::string const key_std = key.toStdString();
    if (_data.displayed_media_key != key_std) {
        _data.displayed_media_key = key_std;
        markDirty();
        emit displayedDataKeyChanged(key);
    }
}

QString MediaWidgetState::displayedDataKey() const {
    return QString::fromStdString(_data.displayed_media_key);
}

// === Viewport State ===

void MediaWidgetState::setZoom(double zoom) {
    // Use epsilon for floating point comparison
    if (std::abs(_data.viewport.zoom - zoom) > 1e-9) {
        _data.viewport.zoom = zoom;
        markDirty();
        emit zoomChanged(zoom);
        emit viewportChanged();
    }
}

void MediaWidgetState::setPan(double x, double y) {
    if (std::abs(_data.viewport.pan_x - x) > 1e-9 || 
        std::abs(_data.viewport.pan_y - y) > 1e-9) {
        _data.viewport.pan_x = x;
        _data.viewport.pan_y = y;
        markDirty();
        emit panChanged(x, y);
        emit viewportChanged();
    }
}

std::pair<double, double> MediaWidgetState::pan() const {
    return {_data.viewport.pan_x, _data.viewport.pan_y};
}

void MediaWidgetState::setCanvasSize(int width, int height) {
    if (_data.viewport.canvas_width != width || 
        _data.viewport.canvas_height != height) {
        _data.viewport.canvas_width = width;
        _data.viewport.canvas_height = height;
        markDirty();
        emit canvasSizeChanged(width, height);
        emit viewportChanged();
    }
}

std::pair<int, int> MediaWidgetState::canvasSize() const {
    return {_data.viewport.canvas_width, _data.viewport.canvas_height};
}

void MediaWidgetState::setViewport(ViewportState const & viewport) {
    bool const zoom_changed = std::abs(_data.viewport.zoom - viewport.zoom) > 1e-9;
    bool const pan_changed = std::abs(_data.viewport.pan_x - viewport.pan_x) > 1e-9 ||
                             std::abs(_data.viewport.pan_y - viewport.pan_y) > 1e-9;
    bool const canvas_changed = _data.viewport.canvas_width != viewport.canvas_width ||
                                _data.viewport.canvas_height != viewport.canvas_height;

    if (zoom_changed || pan_changed || canvas_changed) {
        _data.viewport = viewport;
        markDirty();
        
        if (zoom_changed) {
            emit zoomChanged(viewport.zoom);
        }
        if (pan_changed) {
            emit panChanged(viewport.pan_x, viewport.pan_y);
        }
        if (canvas_changed) {
            emit canvasSizeChanged(viewport.canvas_width, viewport.canvas_height);
        }
        emit viewportChanged();
    }
}

// === Feature Management ===

void MediaWidgetState::setFeatureEnabled(QString const & data_key, QString const & data_type, bool enabled) {
    std::string const key_std = data_key.toStdString();
    std::string const type_std = data_type.toStdString();
    
    bool changed = false;
    
    if (type_std == "line") {
        auto it = _data.line_options.find(key_std);
        if (it != _data.line_options.end()) {
            if (it->second.is_visible() != enabled) {
                it->second.is_visible() = enabled;
                changed = true;
            }
        } else if (enabled) {
            // Create default options if enabling and not exists
            LineDisplayOptions opts;
            opts.is_visible() = true;
            _data.line_options[key_std] = opts;
            changed = true;
        }
    } else if (type_std == "mask") {
        auto it = _data.mask_options.find(key_std);
        if (it != _data.mask_options.end()) {
            if (it->second.is_visible() != enabled) {
                it->second.is_visible() = enabled;
                changed = true;
            }
        } else if (enabled) {
            MaskDisplayOptions opts;
            opts.is_visible() = true;
            _data.mask_options[key_std] = opts;
            changed = true;
        }
    } else if (type_std == "point") {
        auto it = _data.point_options.find(key_std);
        if (it != _data.point_options.end()) {
            if (it->second.is_visible() != enabled) {
                it->second.is_visible() = enabled;
                changed = true;
            }
        } else if (enabled) {
            PointDisplayOptions opts;
            opts.is_visible() = true;
            _data.point_options[key_std] = opts;
            changed = true;
        }
    } else if (type_std == "tensor") {
        auto it = _data.tensor_options.find(key_std);
        if (it != _data.tensor_options.end()) {
            if (it->second.is_visible() != enabled) {
                it->second.is_visible() = enabled;
                changed = true;
            }
        } else if (enabled) {
            TensorDisplayOptions opts;
            opts.is_visible() = true;
            _data.tensor_options[key_std] = opts;
            changed = true;
        }
    } else if (type_std == "interval") {
        auto it = _data.interval_options.find(key_std);
        if (it != _data.interval_options.end()) {
            if (it->second.is_visible() != enabled) {
                it->second.is_visible() = enabled;
                changed = true;
            }
        } else if (enabled) {
            DigitalIntervalDisplayOptions opts;
            opts.is_visible() = true;
            _data.interval_options[key_std] = opts;
            changed = true;
        }
    } else if (type_std == "media") {
        auto it = _data.media_options.find(key_std);
        if (it != _data.media_options.end()) {
            if (it->second.is_visible() != enabled) {
                it->second.is_visible() = enabled;
                changed = true;
            }
        } else if (enabled) {
            MediaDisplayOptions opts;
            opts.is_visible() = true;
            _data.media_options[key_std] = opts;
            changed = true;
        }
    }
    
    if (changed) {
        markDirty();
        emit featureEnabledChanged(data_key, data_type, enabled);
    }
}

bool MediaWidgetState::isFeatureEnabled(QString const & data_key, QString const & data_type) const {
    std::string const key_std = data_key.toStdString();
    std::string const type_std = data_type.toStdString();
    
    if (type_std == "line") {
        auto it = _data.line_options.find(key_std);
        return it != _data.line_options.end() && it->second.is_visible();
    } else if (type_std == "mask") {
        auto it = _data.mask_options.find(key_std);
        return it != _data.mask_options.end() && it->second.is_visible();
    } else if (type_std == "point") {
        auto it = _data.point_options.find(key_std);
        return it != _data.point_options.end() && it->second.is_visible();
    } else if (type_std == "tensor") {
        auto it = _data.tensor_options.find(key_std);
        return it != _data.tensor_options.end() && it->second.is_visible();
    } else if (type_std == "interval") {
        auto it = _data.interval_options.find(key_std);
        return it != _data.interval_options.end() && it->second.is_visible();
    } else if (type_std == "media") {
        auto it = _data.media_options.find(key_std);
        return it != _data.media_options.end() && it->second.is_visible();
    }
    
    return false;
}

QStringList MediaWidgetState::enabledFeatures(QString const & data_type) const {
    QStringList result;
    std::string const type_std = data_type.toStdString();
    
    if (type_std == "line") {
        for (auto const & [key, opts] : _data.line_options) {
            if (opts.is_visible()) {
                result.append(QString::fromStdString(key));
            }
        }
    } else if (type_std == "mask") {
        for (auto const & [key, opts] : _data.mask_options) {
            if (opts.is_visible()) {
                result.append(QString::fromStdString(key));
            }
        }
    } else if (type_std == "point") {
        for (auto const & [key, opts] : _data.point_options) {
            if (opts.is_visible()) {
                result.append(QString::fromStdString(key));
            }
        }
    } else if (type_std == "tensor") {
        for (auto const & [key, opts] : _data.tensor_options) {
            if (opts.is_visible()) {
                result.append(QString::fromStdString(key));
            }
        }
    } else if (type_std == "interval") {
        for (auto const & [key, opts] : _data.interval_options) {
            if (opts.is_visible()) {
                result.append(QString::fromStdString(key));
            }
        }
    } else if (type_std == "media") {
        for (auto const & [key, opts] : _data.media_options) {
            if (opts.is_visible()) {
                result.append(QString::fromStdString(key));
            }
        }
    }
    
    return result;
}

// === Display Options: Line ===

LineDisplayOptions const * MediaWidgetState::lineOptions(QString const & key) const {
    std::string const key_std = key.toStdString();
    auto it = _data.line_options.find(key_std);
    if (it != _data.line_options.end()) {
        return &(it->second);
    }
    return nullptr;
}

/**
* @brief Set line display options for a key
* @param key The data key
* @param options The display options
*/
void MediaWidgetState::setLineOptions(QString const & key, LineDisplayOptions const & options) {
    std::string const key_std = key.toStdString();
    _data.line_options[key_std] = options;
    markDirty();
    emit displayOptionsChanged(key, QStringLiteral("line"));
}

/**
* @brief Remove line display options for a key
* @param key The data key
*/
void MediaWidgetState::removeLineOptions(QString const & key) {
    std::string const key_std = key.toStdString();
    if (_data.line_options.erase(key_std) > 0) {
        markDirty();
        emit displayOptionsRemoved(key, QStringLiteral("line"));
    }
}

// === Display Options: Mask ===

MaskDisplayOptions const * MediaWidgetState::maskOptions(QString const & key) const {
    std::string const key_std = key.toStdString();
    auto it = _data.mask_options.find(key_std);
    if (it != _data.mask_options.end()) {
        return &(it->second);
    }
    return nullptr;
}

/**
* @brief Set mask display options for a key
* @param key The data key
* @param options The display options
*/
void MediaWidgetState::setMaskOptions(QString const & key, MaskDisplayOptions const & options) {
    std::string const key_std = key.toStdString();
    _data.mask_options[key_std] = options;
    markDirty();
    emit displayOptionsChanged(key, QStringLiteral("mask"));
}

    
/**
* @brief Remove mask display options for a key
* @param key The data key
*/
void MediaWidgetState::removeMaskOptions(QString const & key) {
    std::string const key_std = key.toStdString();
    if (_data.mask_options.erase(key_std) > 0) {
        markDirty();
        emit displayOptionsRemoved(key, QStringLiteral("mask"));
    }
}

// === Display Options: Point ===

PointDisplayOptions const * MediaWidgetState::pointOptions(QString const & key) const {
    std::string const key_std = key.toStdString();
    auto it = _data.point_options.find(key_std);
    if (it != _data.point_options.end()) {
        return &(it->second);
    }
    return nullptr;
}

/**
* @brief Set point display options for a key
* @param key The data key
* @param options The display options
*/
void MediaWidgetState::setPointOptions(QString const & key, PointDisplayOptions const & options) {
    std::string const key_std = key.toStdString();
    _data.point_options[key_std] = options;
    markDirty();
    emit displayOptionsChanged(key, QStringLiteral("point"));
}

/**
* @brief Remove point display options for a key
* @param key The data key
*/
void MediaWidgetState::removePointOptions(QString const & key) {
    std::string const key_std = key.toStdString();
    if (_data.point_options.erase(key_std) > 0) {
        markDirty();
        emit displayOptionsRemoved(key, QStringLiteral("point"));
    }
}

// === Display Options: Tensor ===

TensorDisplayOptions const * MediaWidgetState::tensorOptions(QString const & key) const {
    std::string const key_std = key.toStdString();
    auto it = _data.tensor_options.find(key_std);
    if (it != _data.tensor_options.end()) {
        return &(it->second);
    }
    return nullptr;
}

/**
* @brief Set tensor display options for a key
* @param key The data key
* @param options The display options
*/
void MediaWidgetState::setTensorOptions(QString const & key, TensorDisplayOptions const & options) {
    std::string const key_std = key.toStdString();
    _data.tensor_options[key_std] = options;
    markDirty();
    emit displayOptionsChanged(key, QStringLiteral("tensor"));
}

/**
* @brief Remove tensor display options for a key
* @param key The data key
*/
void MediaWidgetState::removeTensorOptions(QString const & key) {
    std::string const key_std = key.toStdString();
    if (_data.tensor_options.erase(key_std) > 0) {
        markDirty();
        emit displayOptionsRemoved(key, QStringLiteral("tensor"));
    }
}

// === Display Options: Interval ===

DigitalIntervalDisplayOptions const * MediaWidgetState::intervalOptions(QString const & key) const {
    std::string const key_std = key.toStdString();
    auto it = _data.interval_options.find(key_std);
    if (it != _data.interval_options.end()) {
        return &(it->second);
    }
    return nullptr;
}

/**
* @brief Set interval display options for a key
* @param key The data key
* @param options The display options
*/
void MediaWidgetState::setIntervalOptions(QString const & key, DigitalIntervalDisplayOptions const & options) {
    std::string const key_std = key.toStdString();
    _data.interval_options[key_std] = options;
    markDirty();
    emit displayOptionsChanged(key, QStringLiteral("interval"));
}


/**
* @brief Remove interval display options for a key
* @param key The data key
*/
void MediaWidgetState::removeIntervalOptions(QString const & key) {
    std::string const key_std = key.toStdString();
    if (_data.interval_options.erase(key_std) > 0) {
        markDirty();
        emit displayOptionsRemoved(key, QStringLiteral("interval"));
    }
}

// === Display Options: Media ===

MediaDisplayOptions const * MediaWidgetState::mediaOptions(QString const & key) const {
    std::string const key_std = key.toStdString();
    auto it = _data.media_options.find(key_std);
    if (it != _data.media_options.end()) {
        return &(it->second);
    }
    return nullptr;
}



std::vector<std::string> MediaWidgetState::mediaOptionKeys() const {
    std::vector<std::string> keys;
    keys.reserve(_data.media_options.size());
    for (auto const & [key, _] : _data.media_options) {
        keys.push_back(key);
    }
    return keys;
}

bool MediaWidgetState::hasMediaOptions(QString const & key) const {
    return _data.media_options.contains(key.toStdString());
}

/**
* @brief Set media display options for a key
* @param key The data key
* @param options The display options
*/
void MediaWidgetState::setMediaOptions(QString const & key, MediaDisplayOptions const & options) {
    std::string const key_std = key.toStdString();
    _data.media_options[key_std] = options;
    markDirty();
    emit displayOptionsChanged(key, QStringLiteral("media"));
}

/**
* @brief Remove media display options for a key
* @param key The data key
*/
void MediaWidgetState::removeMediaOptions(QString const & key) {
    std::string const key_std = key.toStdString();
    if (_data.media_options.erase(key_std) > 0) {
        markDirty();
        emit displayOptionsRemoved(key, QStringLiteral("media"));
    }
}

// === Unified Display Options API ===

void MediaWidgetState::setOptions(QString const & key, DisplayOptionsVariant const & options) {
    std::visit([this, &key](auto const & opts) {
        using T = std::decay_t<decltype(opts)>;
        
        if constexpr (std::is_same_v<T, LineDisplayOptions>) {
            setLineOptions(key, opts);
        } else if constexpr (std::is_same_v<T, MaskDisplayOptions>) {
            setMaskOptions(key, opts);
        } else if constexpr (std::is_same_v<T, PointDisplayOptions>) {
            setPointOptions(key, opts);
        } else if constexpr (std::is_same_v<T, TensorDisplayOptions>) {
            setTensorOptions(key, opts);
        } else if constexpr (std::is_same_v<T, DigitalIntervalDisplayOptions>) {
            setIntervalOptions(key, opts);
        } else if constexpr (std::is_same_v<T, MediaDisplayOptions>) {
            setMediaOptions(key, opts);
        }
    }, options);
}

void MediaWidgetState::removeOptions(QString const & key, DisplayType type) {
    switch (type) {
        case DisplayType::Line:
            removeLineOptions(key);
            break;
        case DisplayType::Mask:
            removeMaskOptions(key);
            break;
        case DisplayType::Point:
            removePointOptions(key);
            break;
        case DisplayType::Tensor:
            removeTensorOptions(key);
            break;
        case DisplayType::Interval:
            removeIntervalOptions(key);
            break;
        case DisplayType::Media:
            removeMediaOptions(key);
            break;
    }
}

// === Interaction Preferences ===

void MediaWidgetState::setLinePrefs(LineInteractionPrefs const & prefs) {
    _data.line_prefs = prefs;
    markDirty();
    emit linePrefsChanged();
}

void MediaWidgetState::setMaskPrefs(MaskInteractionPrefs const & prefs) {
    _data.mask_prefs = prefs;
    markDirty();
    emit maskPrefsChanged();
}

void MediaWidgetState::setPointPrefs(PointInteractionPrefs const & prefs) {
    _data.point_prefs = prefs;
    markDirty();
    emit pointPrefsChanged();
}

// === Text Overlays ===

int MediaWidgetState::addTextOverlay(TextOverlayData overlay) {
    overlay.id = _data.next_overlay_id++;
    _data.text_overlays.push_back(overlay);
    markDirty();
    emit textOverlayAdded(overlay.id);
    return overlay.id;
}

bool MediaWidgetState::removeTextOverlay(int overlay_id) {
    auto it = std::find_if(_data.text_overlays.begin(), _data.text_overlays.end(),
        [overlay_id](TextOverlayData const & o) { return o.id == overlay_id; });
    
    if (it != _data.text_overlays.end()) {
        _data.text_overlays.erase(it);
        markDirty();
        emit textOverlayRemoved(overlay_id);
        return true;
    }
    return false;
}

bool MediaWidgetState::updateTextOverlay(int overlay_id, TextOverlayData const & overlay) {
    auto it = std::find_if(_data.text_overlays.begin(), _data.text_overlays.end(),
        [overlay_id](TextOverlayData const & o) { return o.id == overlay_id; });
    
    if (it != _data.text_overlays.end()) {
        // Preserve the ID
        it->text = overlay.text;
        it->orientation = overlay.orientation;
        it->x_position = overlay.x_position;
        it->y_position = overlay.y_position;
        it->color = overlay.color;
        it->font_size = overlay.font_size;
        it->enabled = overlay.enabled;
        markDirty();
        emit textOverlayUpdated(overlay_id);
        return true;
    }
    return false;
}

void MediaWidgetState::clearTextOverlays() {
    if (!_data.text_overlays.empty()) {
        _data.text_overlays.clear();
        markDirty();
        emit textOverlaysCleared();
    }
}

TextOverlayData const * MediaWidgetState::getTextOverlay(int overlay_id) const {
    auto it = std::find_if(_data.text_overlays.begin(), _data.text_overlays.end(),
        [overlay_id](TextOverlayData const & o) { return o.id == overlay_id; });
    
    if (it != _data.text_overlays.end()) {
        return &(*it);
    }
    return nullptr;
}

// === Active Tool State ===

void MediaWidgetState::setActiveLineMode(LineToolMode mode) {
    if (_data.active_line_mode != mode) {
        _data.active_line_mode = mode;
        markDirty();
        emit activeLineModeChanged(mode);
    }
}

void MediaWidgetState::setActiveMaskMode(MaskToolMode mode) {
    if (_data.active_mask_mode != mode) {
        _data.active_mask_mode = mode;
        markDirty();
        emit activeMaskModeChanged(mode);
    }
}

void MediaWidgetState::setActivePointMode(PointToolMode mode) {
    if (_data.active_point_mode != mode) {
        _data.active_point_mode = mode;
        markDirty();
        emit activePointModeChanged(mode);
    }
}
