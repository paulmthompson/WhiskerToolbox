#ifndef DATA_LOAD_UTILS_HPP
#define DATA_LOAD_UTILS_HPP

/**
 * @file DataLoadUtils.hpp
 * @brief Utility functions for loading data and broadcasting UI configuration
 * 
 * This module provides utility functions that handle the two-phase data loading pattern:
 * 1. Load data into DataManager (triggers DataManager observers)
 * 2. Broadcast UI configuration via EditorRegistry (triggers applyDataDisplayConfig)
 * 
 * This centralizes the load logic so that both MainWindow's JSON loader and
 * BatchProcessing_Widget can share the same code path.
 */

#include "DataManager/DataManagerTypes.hpp"

#include <QString>

#include <functional>
#include <string>
#include <vector>

class DataManager;
class EditorRegistry;

/**
 * @brief Progress callback for data loading operations
 * @param current Current item index (0-based)
 * @param total Total number of items to load
 * @param message Description of current operation
 * @return true to continue loading, false to cancel
 */
using LoadProgressCallback = std::function<bool(int current, int total, std::string const & message)>;

/**
 * @brief Load data from a JSON configuration file and broadcast UI config
 * 
 * This function:
 * 1. Calls DataManager's load_data_from_json_config (triggers DataManager observers)
 * 2. Emits EditorRegistry::applyDataDisplayConfig with the resulting DataInfo
 * 
 * @param dataManager The DataManager to load data into
 * @param registry The EditorRegistry to emit the applyDataDisplayConfig signal
 * @param jsonFilePath Path to the JSON configuration file
 * @param progressCallback Optional progress callback for UI updates
 * @return Vector of DataInfo describing what was loaded
 */
std::vector<DataInfo> loadDataAndBroadcastConfig(
    DataManager * dataManager,
    EditorRegistry * registry,
    std::string const & jsonFilePath,
    LoadProgressCallback progressCallback = nullptr);

/**
 * @brief Load data from JSON content string with a base folder path
 * 
 * This function handles the case where JSON content is provided as a string
 * (e.g., from a text editor) and file paths should be resolved relative to
 * a specified base folder.
 * 
 * This function:
 * 1. Parses JSON content and resolves relative paths against baseFolderPath
 * 2. Creates a temporary JSON file and loads via DataManager
 * 3. Emits EditorRegistry::applyDataDisplayConfig with the resulting DataInfo
 * 
 * @param dataManager The DataManager to load data into
 * @param registry The EditorRegistry to emit the applyDataDisplayConfig signal
 * @param jsonContent JSON configuration as a string
 * @param baseFolderPath Base folder for resolving relative file paths
 * @param progressCallback Optional progress callback for UI updates
 * @return Vector of DataInfo describing what was loaded
 * @throws std::runtime_error on parse errors or invalid configuration
 */
std::vector<DataInfo> loadDataFromJsonContentAndBroadcast(
    DataManager * dataManager,
    EditorRegistry * registry,
    QString const & jsonContent,
    QString const & baseFolderPath,
    LoadProgressCallback progressCallback = nullptr);

/**
 * @brief Reset DataManager and broadcast the reset event
 * 
 * This function:
 * 1. Calls DataManager::reset()
 * 2. Emits EditorRegistry::applyDataDisplayConfig with an empty vector
 *    (indicating all previous UI config should be cleared)
 * 
 * @param dataManager The DataManager to reset
 * @param registry The EditorRegistry to notify
 */
void resetDataManagerAndBroadcast(
    DataManager * dataManager,
    EditorRegistry * registry);

#endif // DATA_LOAD_UTILS_HPP
