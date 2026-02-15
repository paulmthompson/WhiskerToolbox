#include "WorkspaceManager.hpp"

#include "EditorState/EditorRegistry.hpp"
#include "ZoneManager/ZoneConfig.hpp"
#include "ZoneManager/ZoneManager.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTimer>

#include <nlohmann/json.hpp>

#include <iostream>

namespace StateManagement {

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

WorkspaceManager::WorkspaceManager(QString const & config_dir, QObject * parent)
    : QObject(parent)
    , _config_dir(config_dir)
{
}

WorkspaceManager::~WorkspaceManager()
{
    disableAutoSave();
}

// ---------------------------------------------------------------------------
// Collaborators
// ---------------------------------------------------------------------------

void WorkspaceManager::setEditorRegistry(EditorRegistry * registry)
{
    _editor_registry = registry;
}

void WorkspaceManager::setZoneManager(ZoneManager * zone_manager)
{
    _zone_manager = zone_manager;
}

// ---------------------------------------------------------------------------
// Save / Load
// ---------------------------------------------------------------------------

bool WorkspaceManager::saveWorkspace(QString const & file_path)
{
    auto data = _captureCurrentState();
    if (!_writeWorkspaceFile(file_path, data)) {
        return false;
    }

    _current_path = file_path;
    _setDirty(false);
    emit workspaceSaved(file_path);
    emit workspacePathChanged(file_path);
    return true;
}

std::optional<WorkspaceData> WorkspaceManager::readWorkspace(QString const & file_path) const
{
    return _readWorkspaceFile(file_path);
}

void WorkspaceManager::markRestored(QString const & file_path)
{
    _current_path = file_path;
    _setDirty(false);
    emit workspacePathChanged(file_path);
}

// ---------------------------------------------------------------------------
// Data load tracking
// ---------------------------------------------------------------------------

void WorkspaceManager::recordDataLoad(DataLoadEntry entry)
{
    _data_loads.push_back(std::move(entry));
    _setDirty(true);
}

void WorkspaceManager::recordJsonConfigLoad(QString const & config_file_path)
{
    recordDataLoad(DataLoadEntry{
            .loader_type = "json_config",
            .source_path = config_file_path.toStdString()});
}

void WorkspaceManager::recordVideoLoad(QString const & video_path)
{
    recordDataLoad(DataLoadEntry{
            .loader_type = "video",
            .source_path = video_path.toStdString()});
}

void WorkspaceManager::recordImagesLoad(QString const & images_dir)
{
    recordDataLoad(DataLoadEntry{
            .loader_type = "images",
            .source_path = images_dir.toStdString()});
}

void WorkspaceManager::clearDataLoads()
{
    _data_loads.clear();
}

std::vector<DataLoadEntry> const & WorkspaceManager::dataLoads() const
{
    return _data_loads;
}

// ---------------------------------------------------------------------------
// Auto-save (crash recovery)
// ---------------------------------------------------------------------------

void WorkspaceManager::enableAutoSave(int interval_ms)
{
    if (_auto_save_timer) {
        _auto_save_timer->setInterval(interval_ms);
        _auto_save_timer->start();
        return;
    }

    _auto_save_timer = new QTimer(this);
    _auto_save_timer->setInterval(interval_ms);
    connect(_auto_save_timer, &QTimer::timeout, this, &WorkspaceManager::_performAutoSave);
    _auto_save_timer->start();
}

void WorkspaceManager::disableAutoSave()
{
    if (_auto_save_timer) {
        _auto_save_timer->stop();
        delete _auto_save_timer;
        _auto_save_timer = nullptr;
    }
}

bool WorkspaceManager::isAutoSaveEnabled() const
{
    return _auto_save_timer != nullptr && _auto_save_timer->isActive();
}

bool WorkspaceManager::hasRecoveryFile() const
{
    return QFile::exists(recoveryFilePath());
}

std::optional<WorkspaceData> WorkspaceManager::readRecoveryFile() const
{
    return _readWorkspaceFile(recoveryFilePath());
}

void WorkspaceManager::deleteRecoveryFile()
{
    auto const path = recoveryFilePath();
    if (QFile::exists(path)) {
        QFile::remove(path);
    }
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

QString WorkspaceManager::currentWorkspacePath() const
{
    return _current_path;
}

bool WorkspaceManager::hasUnsavedChanges() const
{
    return _is_dirty;
}

void WorkspaceManager::markDirty()
{
    _setDirty(true);
}

void WorkspaceManager::markClean()
{
    _setDirty(false);
}

// ---------------------------------------------------------------------------
// File paths
// ---------------------------------------------------------------------------

QString WorkspaceManager::recoveryFilePath() const
{
    return _config_dir + QStringLiteral("/recovery.wtb");
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

WorkspaceData WorkspaceManager::_captureCurrentState() const
{
    WorkspaceData data;
    data.version = "1.0";

    auto const now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
    data.modified_at = now;
    if (data.created_at.empty()) {
        data.created_at = now;
    }

    data.data_loads = _data_loads;

    if (_editor_registry) {
        data.editor_states_json = _editor_registry->toJson();
    }

    if (_zone_manager) {
        auto const config = _zone_manager->captureCurrentConfig();
        data.zone_layout_json = ZoneConfig::saveToJson(config);
    }

    return data;
}

bool WorkspaceManager::_writeWorkspaceFile(QString const & path,
                                            WorkspaceData const & data) const
{
    try {
        nlohmann::json doc;
        doc["version"] = data.version;
        doc["created_at"] = data.created_at;
        doc["modified_at"] = data.modified_at;

        // Data loads — simple array of objects
        auto loads_arr = nlohmann::json::array();
        for (auto const & entry : data.data_loads) {
            loads_arr.push_back({
                    {"loader_type", entry.loader_type},
                    {"source_path", entry.source_path}});
        }
        doc["data_loads"] = loads_arr;

        // Editor states — embed as parsed JSON object (not escaped string)
        if (!data.editor_states_json.empty()) {
            doc["editor_states"] = nlohmann::json::parse(data.editor_states_json);
        } else {
            doc["editor_states"] = nlohmann::json::object();
        }

        // Zone layout — embed as parsed JSON object
        if (!data.zone_layout_json.empty()) {
            doc["zone_layout"] = nlohmann::json::parse(data.zone_layout_json);
        } else {
            doc["zone_layout"] = nlohmann::json::object();
        }

        // Ensure directory exists
        QDir().mkpath(QFileInfo(path).absolutePath());

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            std::cerr << "WorkspaceManager: cannot open file for writing: "
                      << path.toStdString() << std::endl;
            return false;
        }

        auto const json_str = doc.dump(2); // pretty-print with 2-space indent
        file.write(QByteArray::fromStdString(json_str));
        file.close();
        return true;

    } catch (std::exception const & e) {
        std::cerr << "WorkspaceManager: error writing workspace file: "
                  << e.what() << std::endl;
        return false;
    }
}

std::optional<WorkspaceData> WorkspaceManager::_readWorkspaceFile(QString const & path) const
{
    try {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            std::cerr << "WorkspaceManager: cannot open file for reading: "
                      << path.toStdString() << std::endl;
            return std::nullopt;
        }

        auto const bytes = file.readAll();
        file.close();

        auto doc = nlohmann::json::parse(bytes.toStdString());

        WorkspaceData data;
        data.version = doc.value("version", "1.0");
        data.created_at = doc.value("created_at", "");
        data.modified_at = doc.value("modified_at", "");

        // Data loads
        if (doc.contains("data_loads") && doc["data_loads"].is_array()) {
            for (auto const & entry_json : doc["data_loads"]) {
                DataLoadEntry entry;
                entry.loader_type = entry_json.value("loader_type", "");
                entry.source_path = entry_json.value("source_path", "");
                data.data_loads.push_back(std::move(entry));
            }
        }

        // Editor states — dump sub-object back to string for EditorRegistry::fromJson()
        if (doc.contains("editor_states") && doc["editor_states"].is_object()
            && !doc["editor_states"].empty()) {
            data.editor_states_json = doc["editor_states"].dump();
        }

        // Zone layout — dump sub-object back to string for ZoneConfig::loadFromJson()
        if (doc.contains("zone_layout") && doc["zone_layout"].is_object()
            && !doc["zone_layout"].empty()) {
            data.zone_layout_json = doc["zone_layout"].dump();
        }

        return data;

    } catch (std::exception const & e) {
        std::cerr << "WorkspaceManager: error reading workspace file: "
                  << e.what() << std::endl;
        return std::nullopt;
    }
}

void WorkspaceManager::_performAutoSave()
{
    if (!_editor_registry && !_zone_manager) {
        return; // Not wired up yet
    }

    auto data = _captureCurrentState();
    _writeWorkspaceFile(recoveryFilePath(), data);
}

void WorkspaceManager::_setDirty(bool dirty)
{
    if (_is_dirty != dirty) {
        _is_dirty = dirty;
        emit dirtyChanged(dirty);
    }
}

} // namespace StateManagement
