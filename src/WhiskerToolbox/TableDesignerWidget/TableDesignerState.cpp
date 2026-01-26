#include "TableDesignerState.hpp"

TableDesignerState::TableDesignerState(QObject * parent)
    : EditorState(parent)
{
    // Initialize the instance_id in data from the base class
    _data.instance_id = getInstanceId().toStdString();
}

// ==================== Type Identification ====================

QString TableDesignerState::getDisplayName() const
{
    return QString::fromStdString(_data.display_name);
}

void TableDesignerState::setDisplayName(QString const & name)
{
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

// ==================== Serialization ====================

std::string TableDesignerState::toJson() const
{
    // Include instance_id in serialization for restoration
    TableDesignerStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool TableDesignerState::fromJson(std::string const & json)
{
    auto result = rfl::json::read<TableDesignerStateData>(json);
    if (result) {
        _data = *result;

        // Restore instance ID from serialized data
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }

        emit stateChanged();
        emit currentTableIdChanged(QString::fromStdString(_data.current_table_id));
        emit rowSettingsChanged();
        emit groupSettingsChanged();
        return true;
    }
    return false;
}

// ==================== Table Selection ====================

void TableDesignerState::setCurrentTableId(QString const & table_id)
{
    std::string const table_id_std = table_id.toStdString();
    if (_data.current_table_id != table_id_std) {
        _data.current_table_id = table_id_std;
        markDirty();
        emit currentTableIdChanged(table_id);
    }
}

// ==================== Row Settings ====================

void TableDesignerState::setRowSourceName(QString const & source_name)
{
    std::string const source_std = source_name.toStdString();
    if (_data.row_settings.source_name != source_std) {
        _data.row_settings.source_name = source_std;
        markDirty();
        emit rowSettingsChanged();
    }
}

void TableDesignerState::setCaptureRange(int range)
{
    if (_data.row_settings.capture_range != range) {
        _data.row_settings.capture_range = range;
        markDirty();
        emit rowSettingsChanged();
    }
}

void TableDesignerState::setIntervalMode(IntervalRowMode mode)
{
    if (_data.row_settings.interval_mode != mode) {
        _data.row_settings.interval_mode = mode;
        markDirty();
        emit rowSettingsChanged();
    }
}

void TableDesignerState::setRowSettings(RowSourceSettings const & settings)
{
    bool const source_changed = _data.row_settings.source_name != settings.source_name;
    bool const range_changed = _data.row_settings.capture_range != settings.capture_range;
    bool const mode_changed = _data.row_settings.interval_mode != settings.interval_mode;

    if (source_changed || range_changed || mode_changed) {
        _data.row_settings = settings;
        markDirty();
        emit rowSettingsChanged();
    }
}

// ==================== Group Mode Settings ====================

void TableDesignerState::setGroupModeEnabled(bool enabled)
{
    if (_data.group_settings.enabled != enabled) {
        _data.group_settings.enabled = enabled;
        markDirty();
        emit groupSettingsChanged();
    }
}

void TableDesignerState::setGroupingPattern(QString const & pattern)
{
    std::string const pattern_std = pattern.toStdString();
    if (_data.group_settings.pattern != pattern_std) {
        _data.group_settings.pattern = pattern_std;
        markDirty();
        emit groupSettingsChanged();
    }
}

void TableDesignerState::setGroupSettings(GroupModeSettings const & settings)
{
    bool const enabled_changed = _data.group_settings.enabled != settings.enabled;
    bool const pattern_changed = _data.group_settings.pattern != settings.pattern;

    if (enabled_changed || pattern_changed) {
        _data.group_settings = settings;
        markDirty();
        emit groupSettingsChanged();
    }
}

// ==================== Computer States ====================

void TableDesignerState::setComputerEnabled(QString const & key, bool enabled)
{
    std::string const key_std = key.toStdString();
    auto it = _data.computer_states.find(key_std);
    
    if (it != _data.computer_states.end()) {
        if (it->second.enabled != enabled) {
            it->second.enabled = enabled;
            markDirty();
            emit computerStateChanged(key);
        }
    } else if (enabled) {
        // Create new entry if enabling and not exists
        ComputerStateEntry entry;
        entry.enabled = true;
        _data.computer_states[key_std] = entry;
        markDirty();
        emit computerStateChanged(key);
    }
}

bool TableDesignerState::isComputerEnabled(QString const & key) const
{
    auto it = _data.computer_states.find(key.toStdString());
    return it != _data.computer_states.end() && it->second.enabled;
}

void TableDesignerState::setComputerColumnName(QString const & key, QString const & column_name)
{
    std::string const key_std = key.toStdString();
    std::string const name_std = column_name.toStdString();
    
    auto it = _data.computer_states.find(key_std);
    if (it != _data.computer_states.end()) {
        if (it->second.column_name != name_std) {
            it->second.column_name = name_std;
            markDirty();
            emit computerStateChanged(key);
        }
    } else {
        // Create new entry
        ComputerStateEntry entry;
        entry.column_name = name_std;
        _data.computer_states[key_std] = entry;
        markDirty();
        emit computerStateChanged(key);
    }
}

QString TableDesignerState::computerColumnName(QString const & key) const
{
    auto it = _data.computer_states.find(key.toStdString());
    if (it != _data.computer_states.end()) {
        return QString::fromStdString(it->second.column_name);
    }
    return QString();
}

ComputerStateEntry const * TableDesignerState::getComputerState(QString const & key) const
{
    auto it = _data.computer_states.find(key.toStdString());
    if (it != _data.computer_states.end()) {
        return &it->second;
    }
    return nullptr;
}

void TableDesignerState::setComputerState(QString const & key, ComputerStateEntry const & state)
{
    std::string const key_std = key.toStdString();
    auto it = _data.computer_states.find(key_std);
    
    bool changed = false;
    if (it != _data.computer_states.end()) {
        if (it->second.enabled != state.enabled || it->second.column_name != state.column_name) {
            it->second = state;
            changed = true;
        }
    } else {
        _data.computer_states[key_std] = state;
        changed = true;
    }
    
    if (changed) {
        markDirty();
        emit computerStateChanged(key);
    }
}

bool TableDesignerState::removeComputerState(QString const & key)
{
    auto it = _data.computer_states.find(key.toStdString());
    if (it != _data.computer_states.end()) {
        _data.computer_states.erase(it);
        markDirty();
        emit computerStateChanged(key);
        return true;
    }
    return false;
}

void TableDesignerState::clearComputerStates()
{
    if (!_data.computer_states.empty()) {
        _data.computer_states.clear();
        markDirty();
        emit computerStatesCleared();
    }
}

QStringList TableDesignerState::enabledComputerKeys() const
{
    QStringList result;
    for (auto const & [key, state] : _data.computer_states) {
        if (state.enabled) {
            result.append(QString::fromStdString(key));
        }
    }
    return result;
}

// ==================== Column Order ====================

void TableDesignerState::setColumnOrder(QString const & table_id, QStringList const & column_names)
{
    std::string const table_id_std = table_id.toStdString();
    std::vector<std::string> names_vec;
    names_vec.reserve(column_names.size());
    for (QString const & name : column_names) {
        names_vec.push_back(name.toStdString());
    }
    
    auto it = _data.column_orders.find(table_id_std);
    if (it != _data.column_orders.end()) {
        if (it->second != names_vec) {
            it->second = std::move(names_vec);
            markDirty();
            emit columnOrderChanged(table_id);
        }
    } else {
        _data.column_orders[table_id_std] = std::move(names_vec);
        markDirty();
        emit columnOrderChanged(table_id);
    }
}

QStringList TableDesignerState::columnOrder(QString const & table_id) const
{
    auto it = _data.column_orders.find(table_id.toStdString());
    if (it != _data.column_orders.end()) {
        QStringList result;
        result.reserve(static_cast<int>(it->second.size()));
        for (std::string const & name : it->second) {
            result.append(QString::fromStdString(name));
        }
        return result;
    }
    return {};
}

bool TableDesignerState::removeColumnOrder(QString const & table_id)
{
    auto it = _data.column_orders.find(table_id.toStdString());
    if (it != _data.column_orders.end()) {
        _data.column_orders.erase(it);
        markDirty();
        emit columnOrderChanged(table_id);
        return true;
    }
    return false;
}

void TableDesignerState::clearColumnOrders()
{
    if (!_data.column_orders.empty()) {
        _data.column_orders.clear();
        markDirty();
        // Emit for empty string to indicate all cleared
        emit columnOrderChanged(QString());
    }
}
