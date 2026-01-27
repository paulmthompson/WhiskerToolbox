#include "SeriesOptionsRegistry.hpp"

SeriesOptionsRegistry::SeriesOptionsRegistry(DataViewerStateData * data, QObject * parent)
    : QObject(parent)
    , _data(data)
{
}

// === Type Name Specializations ===

template<>
QString SeriesOptionsRegistry::typeName<AnalogSeriesOptionsData>()
{
    return QStringLiteral("analog");
}

template<>
QString SeriesOptionsRegistry::typeName<DigitalEventSeriesOptionsData>()
{
    return QStringLiteral("event");
}

template<>
QString SeriesOptionsRegistry::typeName<DigitalIntervalSeriesOptionsData>()
{
    return QStringLiteral("interval");
}

// === set() Specializations ===

template<>
void SeriesOptionsRegistry::set<AnalogSeriesOptionsData>(QString const & key, AnalogSeriesOptionsData const & options)
{
    _data->analog_options[key.toStdString()] = options;
    emit optionsChanged(key, typeName<AnalogSeriesOptionsData>());
}

template<>
void SeriesOptionsRegistry::set<DigitalEventSeriesOptionsData>(QString const & key, DigitalEventSeriesOptionsData const & options)
{
    _data->event_options[key.toStdString()] = options;
    emit optionsChanged(key, typeName<DigitalEventSeriesOptionsData>());
}

template<>
void SeriesOptionsRegistry::set<DigitalIntervalSeriesOptionsData>(QString const & key, DigitalIntervalSeriesOptionsData const & options)
{
    _data->interval_options[key.toStdString()] = options;
    emit optionsChanged(key, typeName<DigitalIntervalSeriesOptionsData>());
}

// === get() Specializations ===

template<>
AnalogSeriesOptionsData const * SeriesOptionsRegistry::get<AnalogSeriesOptionsData>(QString const & key) const
{
    auto it = _data->analog_options.find(key.toStdString());
    return it != _data->analog_options.end() ? &it->second : nullptr;
}

template<>
DigitalEventSeriesOptionsData const * SeriesOptionsRegistry::get<DigitalEventSeriesOptionsData>(QString const & key) const
{
    auto it = _data->event_options.find(key.toStdString());
    return it != _data->event_options.end() ? &it->second : nullptr;
}

template<>
DigitalIntervalSeriesOptionsData const * SeriesOptionsRegistry::get<DigitalIntervalSeriesOptionsData>(QString const & key) const
{
    auto it = _data->interval_options.find(key.toStdString());
    return it != _data->interval_options.end() ? &it->second : nullptr;
}

// === getMutable() Specializations ===

template<>
AnalogSeriesOptionsData * SeriesOptionsRegistry::getMutable<AnalogSeriesOptionsData>(QString const & key)
{
    auto it = _data->analog_options.find(key.toStdString());
    return it != _data->analog_options.end() ? &it->second : nullptr;
}

template<>
DigitalEventSeriesOptionsData * SeriesOptionsRegistry::getMutable<DigitalEventSeriesOptionsData>(QString const & key)
{
    auto it = _data->event_options.find(key.toStdString());
    return it != _data->event_options.end() ? &it->second : nullptr;
}

template<>
DigitalIntervalSeriesOptionsData * SeriesOptionsRegistry::getMutable<DigitalIntervalSeriesOptionsData>(QString const & key)
{
    auto it = _data->interval_options.find(key.toStdString());
    return it != _data->interval_options.end() ? &it->second : nullptr;
}

// === remove() Specializations ===

template<>
bool SeriesOptionsRegistry::remove<AnalogSeriesOptionsData>(QString const & key)
{
    auto it = _data->analog_options.find(key.toStdString());
    if (it != _data->analog_options.end()) {
        _data->analog_options.erase(it);
        emit optionsRemoved(key, typeName<AnalogSeriesOptionsData>());
        return true;
    }
    return false;
}

template<>
bool SeriesOptionsRegistry::remove<DigitalEventSeriesOptionsData>(QString const & key)
{
    auto it = _data->event_options.find(key.toStdString());
    if (it != _data->event_options.end()) {
        _data->event_options.erase(it);
        emit optionsRemoved(key, typeName<DigitalEventSeriesOptionsData>());
        return true;
    }
    return false;
}

template<>
bool SeriesOptionsRegistry::remove<DigitalIntervalSeriesOptionsData>(QString const & key)
{
    auto it = _data->interval_options.find(key.toStdString());
    if (it != _data->interval_options.end()) {
        _data->interval_options.erase(it);
        emit optionsRemoved(key, typeName<DigitalIntervalSeriesOptionsData>());
        return true;
    }
    return false;
}

// === has() Specializations ===

template<>
bool SeriesOptionsRegistry::has<AnalogSeriesOptionsData>(QString const & key) const
{
    return _data->analog_options.contains(key.toStdString());
}

template<>
bool SeriesOptionsRegistry::has<DigitalEventSeriesOptionsData>(QString const & key) const
{
    return _data->event_options.contains(key.toStdString());
}

template<>
bool SeriesOptionsRegistry::has<DigitalIntervalSeriesOptionsData>(QString const & key) const
{
    return _data->interval_options.contains(key.toStdString());
}

// === keys() Specializations ===

template<>
QStringList SeriesOptionsRegistry::keys<AnalogSeriesOptionsData>() const
{
    QStringList result;
    result.reserve(static_cast<int>(_data->analog_options.size()));
    for (auto const & [key, _] : _data->analog_options) {
        result.append(QString::fromStdString(key));
    }
    return result;
}

template<>
QStringList SeriesOptionsRegistry::keys<DigitalEventSeriesOptionsData>() const
{
    QStringList result;
    result.reserve(static_cast<int>(_data->event_options.size()));
    for (auto const & [key, _] : _data->event_options) {
        result.append(QString::fromStdString(key));
    }
    return result;
}

template<>
QStringList SeriesOptionsRegistry::keys<DigitalIntervalSeriesOptionsData>() const
{
    QStringList result;
    result.reserve(static_cast<int>(_data->interval_options.size()));
    for (auto const & [key, _] : _data->interval_options) {
        result.append(QString::fromStdString(key));
    }
    return result;
}

// === visibleKeys() Specializations ===

template<>
QStringList SeriesOptionsRegistry::visibleKeys<AnalogSeriesOptionsData>() const
{
    QStringList result;
    for (auto const & [key, opts] : _data->analog_options) {
        if (opts.get_is_visible()) {
            result.append(QString::fromStdString(key));
        }
    }
    return result;
}

template<>
QStringList SeriesOptionsRegistry::visibleKeys<DigitalEventSeriesOptionsData>() const
{
    QStringList result;
    for (auto const & [key, opts] : _data->event_options) {
        if (opts.get_is_visible()) {
            result.append(QString::fromStdString(key));
        }
    }
    return result;
}

template<>
QStringList SeriesOptionsRegistry::visibleKeys<DigitalIntervalSeriesOptionsData>() const
{
    QStringList result;
    for (auto const & [key, opts] : _data->interval_options) {
        if (opts.get_is_visible()) {
            result.append(QString::fromStdString(key));
        }
    }
    return result;
}

// === count() Specializations ===

template<>
int SeriesOptionsRegistry::count<AnalogSeriesOptionsData>() const
{
    return static_cast<int>(_data->analog_options.size());
}

template<>
int SeriesOptionsRegistry::count<DigitalEventSeriesOptionsData>() const
{
    return static_cast<int>(_data->event_options.size());
}

template<>
int SeriesOptionsRegistry::count<DigitalIntervalSeriesOptionsData>() const
{
    return static_cast<int>(_data->interval_options.size());
}

// === notifyChanged() Specializations ===

template<>
void SeriesOptionsRegistry::notifyChanged<AnalogSeriesOptionsData>(QString const & key)
{
    emit optionsChanged(key, typeName<AnalogSeriesOptionsData>());
}

template<>
void SeriesOptionsRegistry::notifyChanged<DigitalEventSeriesOptionsData>(QString const & key)
{
    emit optionsChanged(key, typeName<DigitalEventSeriesOptionsData>());
}

template<>
void SeriesOptionsRegistry::notifyChanged<DigitalIntervalSeriesOptionsData>(QString const & key)
{
    emit optionsChanged(key, typeName<DigitalIntervalSeriesOptionsData>());
}

// === Non-Template Visibility Methods ===

bool SeriesOptionsRegistry::setVisible(QString const & key, QString const & type_name, bool visible)
{
    std::string const std_key = key.toStdString();
    
    if (type_name == QStringLiteral("analog")) {
        auto it = _data->analog_options.find(std_key);
        if (it != _data->analog_options.end()) {
            bool const old_visible = it->second.get_is_visible();
            it->second.is_visible() = visible;
            if (old_visible != visible) {
                emit visibilityChanged(key, type_name, visible);
            }
            return true;
        }
    } else if (type_name == QStringLiteral("event")) {
        auto it = _data->event_options.find(std_key);
        if (it != _data->event_options.end()) {
            bool const old_visible = it->second.get_is_visible();
            it->second.is_visible() = visible;
            if (old_visible != visible) {
                emit visibilityChanged(key, type_name, visible);
            }
            return true;
        }
    } else if (type_name == QStringLiteral("interval")) {
        auto it = _data->interval_options.find(std_key);
        if (it != _data->interval_options.end()) {
            bool const old_visible = it->second.get_is_visible();
            it->second.is_visible() = visible;
            if (old_visible != visible) {
                emit visibilityChanged(key, type_name, visible);
            }
            return true;
        }
    }
    
    return false;
}

bool SeriesOptionsRegistry::isVisible(QString const & key, QString const & type_name) const
{
    std::string const std_key = key.toStdString();
    
    if (type_name == QStringLiteral("analog")) {
        auto it = _data->analog_options.find(std_key);
        return it != _data->analog_options.end() && it->second.get_is_visible();
    } else if (type_name == QStringLiteral("event")) {
        auto it = _data->event_options.find(std_key);
        return it != _data->event_options.end() && it->second.get_is_visible();
    } else if (type_name == QStringLiteral("interval")) {
        auto it = _data->interval_options.find(std_key);
        return it != _data->interval_options.end() && it->second.get_is_visible();
    }
    
    return false;
}
