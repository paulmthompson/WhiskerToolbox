#ifndef BATCHPROCESSING_STATE_HPP
#define BATCHPROCESSING_STATE_HPP

/**
 * @file BatchProcessingState.hpp
 * @brief State class for BatchProcessing_Widget
 * 
 * BatchProcessingState manages the serializable state for the batch processing widget.
 * It handles:
 * - Current top-level folder selection
 * - JSON configuration content
 * - Load history (optional)
 * 
 * The widget uses EditorRegistry to:
 * - Access DataManager for loading
 * - Emit applyDataDisplayConfig after loading
 * 
 * @see EditorState for base class documentation
 * @see BatchProcessing_Widget for the view implementation
 */

#include "EditorState/EditorState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <string>
#include <vector>

class EditorRegistry;

/**
 * @brief Serializable data structure for BatchProcessingState
 */
struct BatchProcessingStateData {
    std::string instance_id;                              ///< Unique instance ID
    std::string display_name = "Batch Processing";        ///< User-visible name
    
    std::string current_top_level_folder;                 ///< Last selected top-level folder
    std::string current_json_file_path;                   ///< Path to last loaded JSON file
    std::string json_content;                             ///< Current JSON configuration text
    
    std::vector<std::string> recent_folders;              ///< Recently used folders (optional)
};

/**
 * @brief State class for BatchProcessing_Widget
 * 
 * BatchProcessingState encapsulates the batch processing widget's state,
 * including folder selection and JSON configuration.
 * 
 * ## Usage
 * 
 * ```cpp
 * auto state = std::make_shared<BatchProcessingState>(registry);
 * 
 * // Set folder and JSON content
 * state->setTopLevelFolder("/path/to/data");
 * state->setJsonContent(jsonString);
 * 
 * // Load data (uses EditorRegistry to access DataManager and emit signals)
 * auto result = state->loadFolder();
 * if (result.success) {
 *     qDebug() << "Loaded" << result.itemCount << "items";
 * }
 * ```
 */
class BatchProcessingState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Result of a load operation
     */
    struct LoadResult {
        bool success = false;
        int itemCount = 0;
        QString errorMessage;
    };

    /**
     * @brief Construct a new BatchProcessingState
     * @param registry EditorRegistry for accessing DataManager and emitting signals
     * @param parent Parent QObject
     */
    explicit BatchProcessingState(EditorRegistry * registry, QObject * parent = nullptr);

    ~BatchProcessingState() override = default;

    // === Type Identification ===

    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("BatchProcessing"); }
    [[nodiscard]] QString getDisplayName() const override;
    void setDisplayName(QString const & name) override;

    // === Serialization ===

    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const & json) override;

    // === Folder Selection ===

    /**
     * @brief Get the current top-level folder
     * @return Path to the top-level folder, or empty string if none selected
     */
    [[nodiscard]] QString topLevelFolder() const;

    /**
     * @brief Set the top-level folder
     * @param folderPath Path to the folder
     */
    void setTopLevelFolder(QString const & folderPath);

    // === JSON Configuration ===

    /**
     * @brief Get the current JSON file path (if loaded from file)
     * @return Path to the JSON file, or empty string if manually entered
     */
    [[nodiscard]] QString jsonFilePath() const;

    /**
     * @brief Set the JSON file path (when loading from file)
     * @param filePath Path to the JSON file
     */
    void setJsonFilePath(QString const & filePath);

    /**
     * @brief Get the current JSON configuration content
     * @return JSON content as string
     */
    [[nodiscard]] QString jsonContent() const;

    /**
     * @brief Set the JSON configuration content
     * @param content JSON content as string
     */
    void setJsonContent(QString const & content);

    // === Load Operations ===

    /**
     * @brief Check if the current state is valid for loading
     * @return true if a folder is selected and valid JSON content exists
     */
    [[nodiscard]] bool canLoad() const;

    /**
     * @brief Load data from the current folder using the current JSON configuration
     * 
     * This method:
     * 1. Resets the DataManager
     * 2. Loads data using loadDataFromJsonContentAndBroadcast utility
     * 3. Emits loadCompleted signal
     * 
     * @param selectedSubfolder Optional subfolder within the top-level folder
     * @return LoadResult with success status and item count
     */
    LoadResult loadFolder(QString const & selectedSubfolder = QString());

signals:
    /**
     * @brief Emitted when the top-level folder changes
     * @param folderPath New folder path
     */
    void topLevelFolderChanged(QString const & folderPath);

    /**
     * @brief Emitted when the JSON configuration changes
     */
    void jsonConfigChanged();

    /**
     * @brief Emitted when a load operation completes
     * @param result The result of the load operation
     */
    void loadCompleted(LoadResult const & result);

private:
    EditorRegistry * _registry;
    BatchProcessingStateData _data;
};

#endif // BATCHPROCESSING_STATE_HPP
