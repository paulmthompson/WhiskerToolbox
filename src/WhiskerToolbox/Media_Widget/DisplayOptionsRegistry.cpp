#include "DisplayOptionsRegistry.hpp"

DisplayOptionsRegistry::DisplayOptionsRegistry(MediaWidgetStateData * data, QObject * parent)
    : QObject(parent)
    , _data(data)
{
}

// === Type Name Specializations ===

template<>
QString DisplayOptionsRegistry::typeName<LineDisplayOptions>()
{
    return QStringLiteral("line");
}

template<>
QString DisplayOptionsRegistry::typeName<MaskDisplayOptions>()
{
    return QStringLiteral("mask");
}

template<>
QString DisplayOptionsRegistry::typeName<PointDisplayOptions>()
{
    return QStringLiteral("point");
}

template<>
QString DisplayOptionsRegistry::typeName<TensorDisplayOptions>()
{
    return QStringLiteral("tensor");
}

template<>
QString DisplayOptionsRegistry::typeName<DigitalIntervalDisplayOptions>()
{
    return QStringLiteral("interval");
}

template<>
QString DisplayOptionsRegistry::typeName<MediaDisplayOptions>()
{
    return QStringLiteral("media");
}

// === set() Specializations ===

template<>
void DisplayOptionsRegistry::set<LineDisplayOptions>(QString const & key, LineDisplayOptions const & options)
{
    _data->line_options[key.toStdString()] = options;
    emit optionsChanged(key, typeName<LineDisplayOptions>());
}

template<>
void DisplayOptionsRegistry::set<MaskDisplayOptions>(QString const & key, MaskDisplayOptions const & options)
{
    _data->mask_options[key.toStdString()] = options;
    emit optionsChanged(key, typeName<MaskDisplayOptions>());
}

template<>
void DisplayOptionsRegistry::set<PointDisplayOptions>(QString const & key, PointDisplayOptions const & options)
{
    _data->point_options[key.toStdString()] = options;
    emit optionsChanged(key, typeName<PointDisplayOptions>());
}

template<>
void DisplayOptionsRegistry::set<TensorDisplayOptions>(QString const & key, TensorDisplayOptions const & options)
{
    _data->tensor_options[key.toStdString()] = options;
    emit optionsChanged(key, typeName<TensorDisplayOptions>());
}

template<>
void DisplayOptionsRegistry::set<DigitalIntervalDisplayOptions>(QString const & key, DigitalIntervalDisplayOptions const & options)
{
    _data->interval_options[key.toStdString()] = options;
    emit optionsChanged(key, typeName<DigitalIntervalDisplayOptions>());
}

template<>
void DisplayOptionsRegistry::set<MediaDisplayOptions>(QString const & key, MediaDisplayOptions const & options)
{
    _data->media_options[key.toStdString()] = options;
    emit optionsChanged(key, typeName<MediaDisplayOptions>());
}

// === get() Specializations ===

template<>
LineDisplayOptions const * DisplayOptionsRegistry::get<LineDisplayOptions>(QString const & key) const
{
    auto it = _data->line_options.find(key.toStdString());
    return it != _data->line_options.end() ? &it->second : nullptr;
}

template<>
MaskDisplayOptions const * DisplayOptionsRegistry::get<MaskDisplayOptions>(QString const & key) const
{
    auto it = _data->mask_options.find(key.toStdString());
    return it != _data->mask_options.end() ? &it->second : nullptr;
}

template<>
PointDisplayOptions const * DisplayOptionsRegistry::get<PointDisplayOptions>(QString const & key) const
{
    auto it = _data->point_options.find(key.toStdString());
    return it != _data->point_options.end() ? &it->second : nullptr;
}

template<>
TensorDisplayOptions const * DisplayOptionsRegistry::get<TensorDisplayOptions>(QString const & key) const
{
    auto it = _data->tensor_options.find(key.toStdString());
    return it != _data->tensor_options.end() ? &it->second : nullptr;
}

template<>
DigitalIntervalDisplayOptions const * DisplayOptionsRegistry::get<DigitalIntervalDisplayOptions>(QString const & key) const
{
    auto it = _data->interval_options.find(key.toStdString());
    return it != _data->interval_options.end() ? &it->second : nullptr;
}

template<>
MediaDisplayOptions const * DisplayOptionsRegistry::get<MediaDisplayOptions>(QString const & key) const
{
    auto it = _data->media_options.find(key.toStdString());
    return it != _data->media_options.end() ? &it->second : nullptr;
}

// === getMutable() Specializations ===

template<>
LineDisplayOptions * DisplayOptionsRegistry::getMutable<LineDisplayOptions>(QString const & key)
{
    auto it = _data->line_options.find(key.toStdString());
    return it != _data->line_options.end() ? &it->second : nullptr;
}

template<>
MaskDisplayOptions * DisplayOptionsRegistry::getMutable<MaskDisplayOptions>(QString const & key)
{
    auto it = _data->mask_options.find(key.toStdString());
    return it != _data->mask_options.end() ? &it->second : nullptr;
}

template<>
PointDisplayOptions * DisplayOptionsRegistry::getMutable<PointDisplayOptions>(QString const & key)
{
    auto it = _data->point_options.find(key.toStdString());
    return it != _data->point_options.end() ? &it->second : nullptr;
}

template<>
TensorDisplayOptions * DisplayOptionsRegistry::getMutable<TensorDisplayOptions>(QString const & key)
{
    auto it = _data->tensor_options.find(key.toStdString());
    return it != _data->tensor_options.end() ? &it->second : nullptr;
}

template<>
DigitalIntervalDisplayOptions * DisplayOptionsRegistry::getMutable<DigitalIntervalDisplayOptions>(QString const & key)
{
    auto it = _data->interval_options.find(key.toStdString());
    return it != _data->interval_options.end() ? &it->second : nullptr;
}

template<>
MediaDisplayOptions * DisplayOptionsRegistry::getMutable<MediaDisplayOptions>(QString const & key)
{
    auto it = _data->media_options.find(key.toStdString());
    return it != _data->media_options.end() ? &it->second : nullptr;
}

// === remove() Specializations ===

template<>
bool DisplayOptionsRegistry::remove<LineDisplayOptions>(QString const & key)
{
    auto it = _data->line_options.find(key.toStdString());
    if (it != _data->line_options.end()) {
        _data->line_options.erase(it);
        emit optionsRemoved(key, typeName<LineDisplayOptions>());
        return true;
    }
    return false;
}

template<>
bool DisplayOptionsRegistry::remove<MaskDisplayOptions>(QString const & key)
{
    auto it = _data->mask_options.find(key.toStdString());
    if (it != _data->mask_options.end()) {
        _data->mask_options.erase(it);
        emit optionsRemoved(key, typeName<MaskDisplayOptions>());
        return true;
    }
    return false;
}

template<>
bool DisplayOptionsRegistry::remove<PointDisplayOptions>(QString const & key)
{
    auto it = _data->point_options.find(key.toStdString());
    if (it != _data->point_options.end()) {
        _data->point_options.erase(it);
        emit optionsRemoved(key, typeName<PointDisplayOptions>());
        return true;
    }
    return false;
}

template<>
bool DisplayOptionsRegistry::remove<TensorDisplayOptions>(QString const & key)
{
    auto it = _data->tensor_options.find(key.toStdString());
    if (it != _data->tensor_options.end()) {
        _data->tensor_options.erase(it);
        emit optionsRemoved(key, typeName<TensorDisplayOptions>());
        return true;
    }
    return false;
}

template<>
bool DisplayOptionsRegistry::remove<DigitalIntervalDisplayOptions>(QString const & key)
{
    auto it = _data->interval_options.find(key.toStdString());
    if (it != _data->interval_options.end()) {
        _data->interval_options.erase(it);
        emit optionsRemoved(key, typeName<DigitalIntervalDisplayOptions>());
        return true;
    }
    return false;
}

template<>
bool DisplayOptionsRegistry::remove<MediaDisplayOptions>(QString const & key)
{
    auto it = _data->media_options.find(key.toStdString());
    if (it != _data->media_options.end()) {
        _data->media_options.erase(it);
        emit optionsRemoved(key, typeName<MediaDisplayOptions>());
        return true;
    }
    return false;
}

// === has() Specializations ===

template<>
bool DisplayOptionsRegistry::has<LineDisplayOptions>(QString const & key) const
{
    return _data->line_options.contains(key.toStdString());
}

template<>
bool DisplayOptionsRegistry::has<MaskDisplayOptions>(QString const & key) const
{
    return _data->mask_options.contains(key.toStdString());
}

template<>
bool DisplayOptionsRegistry::has<PointDisplayOptions>(QString const & key) const
{
    return _data->point_options.contains(key.toStdString());
}

template<>
bool DisplayOptionsRegistry::has<TensorDisplayOptions>(QString const & key) const
{
    return _data->tensor_options.contains(key.toStdString());
}

template<>
bool DisplayOptionsRegistry::has<DigitalIntervalDisplayOptions>(QString const & key) const
{
    return _data->interval_options.contains(key.toStdString());
}

template<>
bool DisplayOptionsRegistry::has<MediaDisplayOptions>(QString const & key) const
{
    return _data->media_options.contains(key.toStdString());
}

// === keys() Specializations ===

template<>
QStringList DisplayOptionsRegistry::keys<LineDisplayOptions>() const
{
    QStringList result;
    result.reserve(static_cast<int>(_data->line_options.size()));
    for (auto const & [key, _] : _data->line_options) {
        result << QString::fromStdString(key);
    }
    return result;
}

template<>
QStringList DisplayOptionsRegistry::keys<MaskDisplayOptions>() const
{
    QStringList result;
    result.reserve(static_cast<int>(_data->mask_options.size()));
    for (auto const & [key, _] : _data->mask_options) {
        result << QString::fromStdString(key);
    }
    return result;
}

template<>
QStringList DisplayOptionsRegistry::keys<PointDisplayOptions>() const
{
    QStringList result;
    result.reserve(static_cast<int>(_data->point_options.size()));
    for (auto const & [key, _] : _data->point_options) {
        result << QString::fromStdString(key);
    }
    return result;
}

template<>
QStringList DisplayOptionsRegistry::keys<TensorDisplayOptions>() const
{
    QStringList result;
    result.reserve(static_cast<int>(_data->tensor_options.size()));
    for (auto const & [key, _] : _data->tensor_options) {
        result << QString::fromStdString(key);
    }
    return result;
}

template<>
QStringList DisplayOptionsRegistry::keys<DigitalIntervalDisplayOptions>() const
{
    QStringList result;
    result.reserve(static_cast<int>(_data->interval_options.size()));
    for (auto const & [key, _] : _data->interval_options) {
        result << QString::fromStdString(key);
    }
    return result;
}

template<>
QStringList DisplayOptionsRegistry::keys<MediaDisplayOptions>() const
{
    QStringList result;
    result.reserve(static_cast<int>(_data->media_options.size()));
    for (auto const & [key, _] : _data->media_options) {
        result << QString::fromStdString(key);
    }
    return result;
}

// === enabledKeys() Specializations ===

template<>
QStringList DisplayOptionsRegistry::enabledKeys<LineDisplayOptions>() const
{
    QStringList result;
    for (auto const & [key, opts] : _data->line_options) {
        if (opts.is_visible()) {
            result << QString::fromStdString(key);
        }
    }
    return result;
}

template<>
QStringList DisplayOptionsRegistry::enabledKeys<MaskDisplayOptions>() const
{
    QStringList result;
    for (auto const & [key, opts] : _data->mask_options) {
        if (opts.is_visible()) {
            result << QString::fromStdString(key);
        }
    }
    return result;
}

template<>
QStringList DisplayOptionsRegistry::enabledKeys<PointDisplayOptions>() const
{
    QStringList result;
    for (auto const & [key, opts] : _data->point_options) {
        if (opts.is_visible()) {
            result << QString::fromStdString(key);
        }
    }
    return result;
}

template<>
QStringList DisplayOptionsRegistry::enabledKeys<TensorDisplayOptions>() const
{
    QStringList result;
    for (auto const & [key, opts] : _data->tensor_options) {
        if (opts.is_visible()) {
            result << QString::fromStdString(key);
        }
    }
    return result;
}

template<>
QStringList DisplayOptionsRegistry::enabledKeys<DigitalIntervalDisplayOptions>() const
{
    QStringList result;
    for (auto const & [key, opts] : _data->interval_options) {
        if (opts.is_visible()) {
            result << QString::fromStdString(key);
        }
    }
    return result;
}

template<>
QStringList DisplayOptionsRegistry::enabledKeys<MediaDisplayOptions>() const
{
    QStringList result;
    for (auto const & [key, opts] : _data->media_options) {
        if (opts.is_visible()) {
            result << QString::fromStdString(key);
        }
    }
    return result;
}

// === count() Specializations ===

template<>
int DisplayOptionsRegistry::count<LineDisplayOptions>() const
{
    return static_cast<int>(_data->line_options.size());
}

template<>
int DisplayOptionsRegistry::count<MaskDisplayOptions>() const
{
    return static_cast<int>(_data->mask_options.size());
}

template<>
int DisplayOptionsRegistry::count<PointDisplayOptions>() const
{
    return static_cast<int>(_data->point_options.size());
}

template<>
int DisplayOptionsRegistry::count<TensorDisplayOptions>() const
{
    return static_cast<int>(_data->tensor_options.size());
}

template<>
int DisplayOptionsRegistry::count<DigitalIntervalDisplayOptions>() const
{
    return static_cast<int>(_data->interval_options.size());
}

template<>
int DisplayOptionsRegistry::count<MediaDisplayOptions>() const
{
    return static_cast<int>(_data->media_options.size());
}

// === notifyChanged() Specializations ===

template<>
void DisplayOptionsRegistry::notifyChanged<LineDisplayOptions>(QString const & key)
{
    emit optionsChanged(key, typeName<LineDisplayOptions>());
}

template<>
void DisplayOptionsRegistry::notifyChanged<MaskDisplayOptions>(QString const & key)
{
    emit optionsChanged(key, typeName<MaskDisplayOptions>());
}

template<>
void DisplayOptionsRegistry::notifyChanged<PointDisplayOptions>(QString const & key)
{
    emit optionsChanged(key, typeName<PointDisplayOptions>());
}

template<>
void DisplayOptionsRegistry::notifyChanged<TensorDisplayOptions>(QString const & key)
{
    emit optionsChanged(key, typeName<TensorDisplayOptions>());
}

template<>
void DisplayOptionsRegistry::notifyChanged<DigitalIntervalDisplayOptions>(QString const & key)
{
    emit optionsChanged(key, typeName<DigitalIntervalDisplayOptions>());
}

template<>
void DisplayOptionsRegistry::notifyChanged<MediaDisplayOptions>(QString const & key)
{
    emit optionsChanged(key, typeName<MediaDisplayOptions>());
}

// === Visibility Convenience Methods ===

bool DisplayOptionsRegistry::setVisible(QString const & key, QString const & type_name, bool visible)
{
    bool found = false;
    bool old_visible = false;

    if (type_name == QStringLiteral("line")) {
        if (auto * opts = getMutable<LineDisplayOptions>(key)) {
            old_visible = opts->is_visible();
            opts->is_visible() = visible;
            found = true;
        }
    } else if (type_name == QStringLiteral("mask")) {
        if (auto * opts = getMutable<MaskDisplayOptions>(key)) {
            old_visible = opts->is_visible();
            opts->is_visible() = visible;
            found = true;
        }
    } else if (type_name == QStringLiteral("point")) {
        if (auto * opts = getMutable<PointDisplayOptions>(key)) {
            old_visible = opts->is_visible();
            opts->is_visible() = visible;
            found = true;
        }
    } else if (type_name == QStringLiteral("tensor")) {
        if (auto * opts = getMutable<TensorDisplayOptions>(key)) {
            old_visible = opts->is_visible();
            opts->is_visible() = visible;
            found = true;
        }
    } else if (type_name == QStringLiteral("interval")) {
        if (auto * opts = getMutable<DigitalIntervalDisplayOptions>(key)) {
            old_visible = opts->is_visible();
            opts->is_visible() = visible;
            found = true;
        }
    } else if (type_name == QStringLiteral("media")) {
        if (auto * opts = getMutable<MediaDisplayOptions>(key)) {
            old_visible = opts->is_visible();
            opts->is_visible() = visible;
            found = true;
        }
    }

    if (found && old_visible != visible) {
        emit visibilityChanged(key, type_name, visible);
        emit optionsChanged(key, type_name);
    }

    return found;
}

bool DisplayOptionsRegistry::isVisible(QString const & key, QString const & type_name) const
{
    if (type_name == QStringLiteral("line")) {
        if (auto const * opts = get<LineDisplayOptions>(key)) {
            return opts->is_visible();
        }
    } else if (type_name == QStringLiteral("mask")) {
        if (auto const * opts = get<MaskDisplayOptions>(key)) {
            return opts->is_visible();
        }
    } else if (type_name == QStringLiteral("point")) {
        if (auto const * opts = get<PointDisplayOptions>(key)) {
            return opts->is_visible();
        }
    } else if (type_name == QStringLiteral("tensor")) {
        if (auto const * opts = get<TensorDisplayOptions>(key)) {
            return opts->is_visible();
        }
    } else if (type_name == QStringLiteral("interval")) {
        if (auto const * opts = get<DigitalIntervalDisplayOptions>(key)) {
            return opts->is_visible();
        }
    } else if (type_name == QStringLiteral("media")) {
        if (auto const * opts = get<MediaDisplayOptions>(key)) {
            return opts->is_visible();
        }
    }

    return false;
}
