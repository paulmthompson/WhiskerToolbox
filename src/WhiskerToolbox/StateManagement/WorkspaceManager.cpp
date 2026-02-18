#include "WorkspaceManager.hpp"

#include "EditorState/EditorRegistry.hpp"
#include "PathResolver.hpp"
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

void WorkspaceManager::setDockStateCaptureCallback(std::function<std::string()> callback)
{
    _dock_state_capture = std::move(callback);
}

// ---------------------------------------------------------------------------
// Save / Load
// ---------------------------------------------------------------------------

bool WorkspaceManager::saveWorkspace(QString const & file_path)
{
    auto data = _captureCurrentState();

    // Convert absolute paths to workspace-relative before writing
    _convertToRelativePaths(data, file_path);

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
    auto data = _readWorkspaceFile(file_path);
    if (data) {
        // Resolve relative paths to absolute based on workspace file location
        _resolveRelativePaths(*data, file_path);
    }
    return data;
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
// Transform Pipeline Tracking
// ---------------------------------------------------------------------------

void WorkspaceManager::recordAppliedPipeline(std::string const & pipeline_json)
{
    _applied_pipelines.push_back(pipeline_json);
    _setDirty(true);
}

void WorkspaceManager::clearAppliedPipelines()
{
    _applied_pipelines.clear();
}

std::vector<std::string> const & WorkspaceManager::appliedPipelines() const
{
    return _applied_pipelines;
}

// ---------------------------------------------------------------------------
// Table Definition Tracking
// ---------------------------------------------------------------------------

void WorkspaceManager::recordTableDefinition(std::string const & table_json)
{
    _table_definitions.push_back(table_json);
    _setDirty(true);
}

void WorkspaceManager::clearTableDefinitions()
{
    _table_definitions.clear();
}

std::vector<std::string> const & WorkspaceManager::tableDefinitions() const
{
    return _table_definitions;
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
    data.applied_pipelines = _applied_pipelines;
    data.table_definitions = _table_definitions;

    if (_editor_registry) {
        data.editor_states_json = _editor_registry->toJson();
    }

    if (_zone_manager) {
        auto const config = _zone_manager->captureCurrentConfig();
        data.zone_layout_json = ZoneConfig::saveToJson(config);
    }

    // Capture ADS dock state for exact widget positions/splits
    if (_dock_state_capture) {
        data.dock_state_base64 = _dock_state_capture();
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

        // Data loads — include both source_path and relative_path
        auto loads_arr = nlohmann::json::array();
        for (auto const & entry : data.data_loads) {
            nlohmann::json entry_json;
            entry_json["loader_type"] = entry.loader_type;
            entry_json["source_path"] = entry.source_path;
            if (!entry.relative_path.empty()) {
                entry_json["relative_path"] = entry.relative_path;
            }
            loads_arr.push_back(entry_json);
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

        // ADS dock state for exact widget positions/splits
        if (!data.dock_state_base64.empty()) {
            doc["dock_state"] = data.dock_state_base64;
        }

        // Applied pipelines
        auto pipelines_arr = nlohmann::json::array();
        for (auto const & pj : data.applied_pipelines) {
            try {
                pipelines_arr.push_back(nlohmann::json::parse(pj));
            } catch (...) {
                pipelines_arr.push_back(pj);
            }
        }
        doc["applied_pipelines"] = pipelines_arr;

        // Table definitions
        auto tables_arr = nlohmann::json::array();
        for (auto const & tj : data.table_definitions) {
            try {
                tables_arr.push_back(nlohmann::json::parse(tj));
            } catch (...) {
                tables_arr.push_back(tj);
            }
        }
        doc["table_definitions"] = tables_arr;

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
                entry.relative_path = entry_json.value("relative_path", "");
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

        // ADS dock state
        if (doc.contains("dock_state") && doc["dock_state"].is_string()) {
            data.dock_state_base64 = doc["dock_state"].get<std::string>();
        }

        // Applied pipelines
        if (doc.contains("applied_pipelines") && doc["applied_pipelines"].is_array()) {
            for (auto const & pj : doc["applied_pipelines"]) {
                data.applied_pipelines.push_back(pj.is_string() ? pj.get<std::string>() : pj.dump());
            }
        }

        // Table definitions
        if (doc.contains("table_definitions") && doc["table_definitions"].is_array()) {
            for (auto const & tj : doc["table_definitions"]) {
                data.table_definitions.push_back(tj.is_string() ? tj.get<std::string>() : tj.dump());
            }
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

void WorkspaceManager::_convertToRelativePaths(WorkspaceData & data,
                                                QString const & workspace_path) const
{
    auto const ws_dir = QFileInfo(workspace_path).absolutePath().toStdString();
    for (auto & entry : data.data_loads) {
        entry.relative_path = PathResolver::toRelative(entry.source_path, ws_dir);
    }
}

void WorkspaceManager::_resolveRelativePaths(WorkspaceData & data,
                                              QString const & workspace_path) const
{
    auto const ws_dir = QFileInfo(workspace_path).absolutePath().toStdString();
    for (auto & entry : data.data_loads) {
        // If source_path doesn't exist but relative_path does, resolve it
        if (!entry.relative_path.empty()) {
            auto const resolved = PathResolver::toAbsolute(entry.relative_path, ws_dir);
            if (PathResolver::fileExists(resolved)) {
                entry.source_path = resolved;
            }
            // If original source_path still exists, keep it (takes priority)
        }
    }
}

} // namespace StateManagement
