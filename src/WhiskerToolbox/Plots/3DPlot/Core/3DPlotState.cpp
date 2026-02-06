#include "3DPlotState.hpp"

#include <rfl/json.hpp>

ThreeDPlotState::ThreeDPlotState(QObject * parent)
    : EditorState(parent)
{
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();
}

QString ThreeDPlotState::getDisplayName() const
{
    return QString::fromStdString(_data.display_name);
}

void ThreeDPlotState::setDisplayName(QString const & name)
{
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string ThreeDPlotState::toJson() const
{
    // Include instance_id in serialization for restoration
    ThreeDPlotStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool ThreeDPlotState::fromJson(std::string const & json)
{
    auto result = rfl::json::read<ThreeDPlotStateData>(json);
    if (result) {
        _data = *result;

        // Restore instance ID from serialized data
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }

        // Emit all signals to ensure UI updates
        emit stateChanged();
        return true;
    }
    return false;
}

QString ThreeDPlotState::getActivePointDataKey() const
{
    return QString::fromStdString(_data.active_point_data_key);
}

void ThreeDPlotState::setActivePointDataKey(QString const & key)
{
    std::string key_str = key.toStdString();
    if (_data.active_point_data_key != key_str) {
        _data.active_point_data_key = key_str;
        markDirty();
        emit activePointDataKeyChanged(key);
        emit stateChanged();
    }
}

void ThreeDPlotState::addPlotDataKey(QString const & data_key)
{
    std::string key_str = data_key.toStdString();
    if (_data.plot_data_keys.find(key_str) != _data.plot_data_keys.end()) {
        return;  // Already exists
    }

    ThreeDPlotDataOptions options;
    options.data_key = key_str;
    _data.plot_data_keys[key_str] = options;
    markDirty();
    emit plotDataKeyAdded(data_key);
    emit stateChanged();
}

void ThreeDPlotState::removePlotDataKey(QString const & data_key)
{
    std::string key_str = data_key.toStdString();
    auto it = _data.plot_data_keys.find(key_str);
    if (it != _data.plot_data_keys.end()) {
        _data.plot_data_keys.erase(it);
        markDirty();
        emit plotDataKeyRemoved(data_key);
        emit stateChanged();
    }
}

std::vector<QString> ThreeDPlotState::getPlotDataKeys() const
{
    std::vector<QString> keys;
    keys.reserve(_data.plot_data_keys.size());
    for (auto const & [key, _] : _data.plot_data_keys) {
        keys.push_back(QString::fromStdString(key));
    }
    return keys;
}

std::optional<ThreeDPlotDataOptions> ThreeDPlotState::getPlotDataKeyOptions(QString const & data_key) const
{
    std::string key_str = data_key.toStdString();
    auto it = _data.plot_data_keys.find(key_str);
    if (it != _data.plot_data_keys.end()) {
        return it->second;
    }
    return std::nullopt;
}
