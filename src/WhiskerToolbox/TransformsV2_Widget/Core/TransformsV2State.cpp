#include "TransformsV2State.hpp"

#include <rfl/json.hpp>

#include <iostream>

TransformsV2State::TransformsV2State(std::shared_ptr<DataManager> data_manager,
                                     QObject * parent)
    : EditorState(parent)
    , _data_manager(std::move(data_manager)) {
}

QString TransformsV2State::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void TransformsV2State::setDisplayName(QString const & name) {
    auto const name_std = name.toStdString();
    if (_data.display_name != name_std) {
        _data.display_name = name_std;
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string TransformsV2State::toJson() const {
    TransformsV2StateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool TransformsV2State::fromJson(std::string const & json) {
    auto result = rfl::json::read<TransformsV2StateData>(json);
    if (result) {
        _data = *result;
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }
        emit stateChanged();
        return true;
    }
    std::cerr << "TransformsV2State::fromJson: Failed to parse JSON" << std::endl;
    return false;
}
