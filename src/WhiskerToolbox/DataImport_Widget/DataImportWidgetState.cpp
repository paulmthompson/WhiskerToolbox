#include "DataImportWidgetState.hpp"

DataImportWidgetState::DataImportWidgetState(QObject * parent)
    : EditorState(parent) {
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();
}

QString DataImportWidgetState::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void DataImportWidgetState::setDisplayName(QString const & name) {
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string DataImportWidgetState::toJson() const {
    // Include instance_id in serialization for restoration
    DataImportWidgetStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool DataImportWidgetState::fromJson(std::string const & json) {
    auto result = rfl::json::read<DataImportWidgetStateData>(json);
    if (result) {
        _data = *result;

        // Restore instance ID from serialized data
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }

        emit stateChanged();
        emit selectedImportTypeChanged(QString::fromStdString(_data.selected_import_type));
        emit lastUsedDirectoryChanged(QString::fromStdString(_data.last_used_directory));

        // Emit format preference changes
        for (auto const & [data_type, format] : _data.format_preferences) {
            emit formatPreferenceChanged(QString::fromStdString(data_type),
                                          QString::fromStdString(format));
        }
        return true;
    }
    return false;
}

void DataImportWidgetState::setSelectedImportType(QString const & type) {
    std::string const type_std = type.toStdString();
    if (_data.selected_import_type != type_std) {
        _data.selected_import_type = type_std;
        markDirty();
        emit selectedImportTypeChanged(type);
    }
}

QString DataImportWidgetState::selectedImportType() const {
    return QString::fromStdString(_data.selected_import_type);
}

void DataImportWidgetState::setLastUsedDirectory(QString const & dir) {
    std::string const dir_std = dir.toStdString();
    if (_data.last_used_directory != dir_std) {
        _data.last_used_directory = dir_std;
        markDirty();
        emit lastUsedDirectoryChanged(dir);
    }
}

QString DataImportWidgetState::lastUsedDirectory() const {
    return QString::fromStdString(_data.last_used_directory);
}

void DataImportWidgetState::setFormatPreference(QString const & data_type, QString const & format) {
    std::string const type_std = data_type.toStdString();
    std::string const format_std = format.toStdString();

    auto it = _data.format_preferences.find(type_std);
    if (it == _data.format_preferences.end() || it->second != format_std) {
        _data.format_preferences[type_std] = format_std;
        markDirty();
        emit formatPreferenceChanged(data_type, format);
    }
}

QString DataImportWidgetState::formatPreference(QString const & data_type) const {
    std::string const type_std = data_type.toStdString();
    auto it = _data.format_preferences.find(type_std);
    if (it != _data.format_preferences.end()) {
        return QString::fromStdString(it->second);
    }
    return QString();
}
