#include "BatchProcessingState.hpp"

#include "EditorState/EditorRegistry.hpp"
#include "utils/DataLoadUtils.hpp"

#include "DataManager/DataManager.hpp"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonParseError>

BatchProcessingState::BatchProcessingState(EditorRegistry * registry,
                                           std::shared_ptr<DataManager> data_manager,
                                           QObject * parent)
    : EditorState(parent),
      _registry(registry),
      _data_manager(std::move(data_manager)) {
    _data.instance_id = getInstanceId().toStdString();
}

QString BatchProcessingState::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void BatchProcessingState::setDisplayName(QString const & name) {
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string BatchProcessingState::toJson() const {
    return rfl::json::write(_data);
}

bool BatchProcessingState::fromJson(std::string const & json) {
    auto result = rfl::json::read<BatchProcessingStateData>(json);
    if (result) {
        _data = *result;
        setInstanceId(QString::fromStdString(_data.instance_id));
        emit stateChanged();
        return true;
    }
    return false;
}

QString BatchProcessingState::topLevelFolder() const {
    return QString::fromStdString(_data.current_top_level_folder);
}

void BatchProcessingState::setTopLevelFolder(QString const & folderPath) {
    if (_data.current_top_level_folder != folderPath.toStdString()) {
        _data.current_top_level_folder = folderPath.toStdString();

        // Add to recent folders if not already present
        auto it = std::find(_data.recent_folders.begin(), _data.recent_folders.end(),
                            folderPath.toStdString());
        if (it == _data.recent_folders.end()) {
            _data.recent_folders.insert(_data.recent_folders.begin(), folderPath.toStdString());
            // Keep only last 10 recent folders
            if (_data.recent_folders.size() > 10) {
                _data.recent_folders.resize(10);
            }
        }

        markDirty();
        emit topLevelFolderChanged(folderPath);
    }
}

QString BatchProcessingState::jsonFilePath() const {
    return QString::fromStdString(_data.current_json_file_path);
}

void BatchProcessingState::setJsonFilePath(QString const & filePath) {
    if (_data.current_json_file_path != filePath.toStdString()) {
        _data.current_json_file_path = filePath.toStdString();
        markDirty();
    }
}

QString BatchProcessingState::jsonContent() const {
    return QString::fromStdString(_data.json_content);
}

void BatchProcessingState::setJsonContent(QString const & content) {
    if (_data.json_content != content.toStdString()) {
        _data.json_content = content.toStdString();
        markDirty();
        emit jsonConfigChanged();
    }
}

bool BatchProcessingState::canLoad() const {
    // Need a folder selected and non-empty JSON content
    if (_data.current_top_level_folder.empty()) {
        return false;
    }

    // Check that JSON content is not empty and is valid JSON
    QString content = QString::fromStdString(_data.json_content).trimmed();
    if (content.isEmpty()) {
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument::fromJson(content.toUtf8(), &parseError);
    return parseError.error == QJsonParseError::NoError;
}

BatchProcessingState::LoadResult BatchProcessingState::loadFolder(QString const & selectedSubfolder) {
    LoadResult result;

    if (!_registry) {
        result.errorMessage = "EditorRegistry not available";
        return result;
    }

    if (!_data_manager) {
        result.errorMessage = "DataManager not available";
        return result;
    }

    // Determine the base folder path
    QString baseFolderPath;
    if (selectedSubfolder.isEmpty()) {
        baseFolderPath = QString::fromStdString(_data.current_top_level_folder);
    } else {
        baseFolderPath = selectedSubfolder;
    }

    if (baseFolderPath.isEmpty()) {
        result.errorMessage = "No folder selected";
        return result;
    }

    QString jsonContent = QString::fromStdString(_data.json_content).trimmed();
    if (jsonContent.isEmpty()) {
        result.errorMessage = "No JSON configuration provided";
        return result;
    }

    try {
        qDebug() << "BatchProcessingState: Loading folder:" << baseFolderPath;
        qDebug() << "BatchProcessingState: JSON content length:" << jsonContent.length();

        // Reset DataManager before loading new data
        resetDataManagerAndBroadcast(_data_manager.get(), _registry);

        // Load data using the utility function
        // This handles both phases:
        // 1. Load data into DataManager (triggers observers)
        // 2. Emit applyDataDisplayConfig signal
        auto dataInfo = loadDataFromJsonContentAndBroadcast(
                _data_manager.get(),
                _registry,
                jsonContent,
                baseFolderPath);

        result.success = true;
        result.itemCount = static_cast<int>(dataInfo.size());

        qDebug() << "BatchProcessingState: Successfully loaded" << result.itemCount << "items";

    } catch (std::exception const & e) {
        result.errorMessage = QString::fromUtf8(e.what());
        qWarning() << "BatchProcessingState: Load error:" << result.errorMessage;
    }

    emit loadCompleted(result);
    return result;
}
