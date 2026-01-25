#include "DataLoadUtils.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/EditorRegistry.hpp"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTemporaryFile>

#include <stdexcept>

namespace {

/**
 * @brief Convert DataInfo vector to DataDisplayConfig vector
 * 
 * This conversion is needed because EditorRegistry uses DataDisplayConfig
 * to avoid circular dependencies with DataManager.
 */
std::vector<DataDisplayConfig> toDisplayConfig(std::vector<DataInfo> const & dataInfo) {
    std::vector<DataDisplayConfig> result;
    result.reserve(dataInfo.size());
    for (auto const & info : dataInfo) {
        result.push_back({info.key, info.data_class, info.color});
    }
    return result;
}

} // namespace

std::vector<DataInfo> loadDataAndBroadcastConfig(
    DataManager * dataManager,
    EditorRegistry * registry,
    std::string const & jsonFilePath,
    LoadProgressCallback progressCallback) {
    
    if (!dataManager) {
        throw std::runtime_error("DataManager is null");
    }

    std::vector<DataInfo> dataInfo;

    // Phase 1: Load data into DataManager
    // This triggers DataManager's internal observers (untyped _notifyObservers)
    if (progressCallback) {
        dataInfo = load_data_from_json_config(dataManager, jsonFilePath, progressCallback);
    } else {
        dataInfo = load_data_from_json_config(dataManager, jsonFilePath);
    }

    // Phase 2: Broadcast UI configuration via EditorRegistry signal
    // Widgets connect to this to apply colors, styles, etc.
    if (registry && !dataInfo.empty()) {
        emit registry->applyDataDisplayConfig(toDisplayConfig(dataInfo));
    }

    return dataInfo;
}

std::vector<DataInfo> loadDataFromJsonContentAndBroadcast(
    DataManager * dataManager,
    EditorRegistry * registry,
    QString const & jsonContent,
    QString const & baseFolderPath,
    LoadProgressCallback progressCallback) {
    
    if (!dataManager) {
        throw std::runtime_error("DataManager is null");
    }

    // Validate input parameters
    if (jsonContent.trimmed().isEmpty()) {
        throw std::runtime_error("JSON content is empty");
    }

    if (baseFolderPath.isEmpty()) {
        throw std::runtime_error("Base folder path is empty");
    }

    // Parse the JSON content
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonContent.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        throw std::runtime_error(
            QString("JSON parse error: %1").arg(parseError.errorString()).toStdString());
    }

    // Check if it's an array (as expected by the DataManager function)
    if (!jsonDoc.isArray()) {
        throw std::runtime_error("JSON must be an array of data configurations");
    }

    // Create a temporary JSON file with the modified paths
    QTemporaryFile tempFile;
    tempFile.setFileTemplate(QDir::temp().filePath("batch_config_XXXXXX.json"));
    tempFile.setAutoRemove(false); // Keep file for debugging if needed

    if (!tempFile.open()) {
        throw std::runtime_error("Could not create temporary JSON file");
    }

    // Create a modified JSON with paths resolved relative to the base folder
    QJsonArray jsonArray = jsonDoc.array();
    QJsonArray modifiedArray;

    for (QJsonValue const & value : jsonArray) {
        QJsonObject item = value.toObject();

        // If the filepath is relative, make it absolute using the base folder
        if (item.contains("filepath")) {
            QString filepath = item["filepath"].toString();
            QFileInfo fileInfo(filepath);

            if (fileInfo.isRelative()) {
                QString absolutePath = QDir(baseFolderPath).absoluteFilePath(filepath);
                item["filepath"] = absolutePath;
            }
        }

        modifiedArray.append(item);
    }

    // Write the modified JSON to the temporary file
    QJsonDocument modifiedDoc(modifiedArray);
    QByteArray jsonData = modifiedDoc.toJson(QJsonDocument::Indented);

    if (tempFile.write(jsonData) == -1) {
        tempFile.close();
        throw std::runtime_error("Failed to write JSON data to temporary file");
    }

    tempFile.flush();
    tempFile.close();

    qDebug() << "DataLoadUtils: Temporary JSON file created at:" << tempFile.fileName();

    // Load using the standard function
    std::vector<DataInfo> dataInfo;
    try {
        if (progressCallback) {
            dataInfo = load_data_from_json_config(
                dataManager, tempFile.fileName().toStdString(), progressCallback);
        } else {
            dataInfo = load_data_from_json_config(
                dataManager, tempFile.fileName().toStdString());
        }
    } catch (...) {
        // Clean up temp file before re-throwing
        QFile::remove(tempFile.fileName());
        throw;
    }

    // Clean up the temporary file
    QFile::remove(tempFile.fileName());

    // Phase 2: Broadcast UI configuration via EditorRegistry signal
    if (registry && !dataInfo.empty()) {
        emit registry->applyDataDisplayConfig(toDisplayConfig(dataInfo));
    }

    return dataInfo;
}

void resetDataManagerAndBroadcast(
    DataManager * dataManager,
    EditorRegistry * registry) {
    
    if (!dataManager) {
        return;
    }

    // Reset the DataManager (triggers its internal observers)
    dataManager->reset();

    // Broadcast empty config to signal that all UI config should be cleared
    if (registry) {
        emit registry->applyDataDisplayConfig({});
    }
}
