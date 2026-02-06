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
