#ifndef DATA_IMPORT_WIDGET_STATE_HPP
#define DATA_IMPORT_WIDGET_STATE_HPP

/**
 * @file DataImportWidgetState.hpp
 * @brief State class for DataImport_Widget
 * 
 * DataImportWidgetState manages the serializable state for the DataImport_Widget,
 * enabling workspace save/restore and responding to data focus changes via
 * SelectionContext.
 * 
 * This widget implements the "Passive Awareness" pattern - it observes
 * SelectionContext::dataFocusChanged and updates its displayed loader widget
 * based on the type of data currently focused.
 * 
 * State tracked:
 * - Selected import data type (determines which loader widget is shown)
 * - Last used directory (for file dialogs)
 * - Per-type format preferences (e.g., "LineData" -> "CSV")
 * 
 * @see EditorState for base class documentation
 * @see DataFocusAware for passive awareness pattern
 * @see SelectionContext for inter-widget communication
 */

#include "EditorState/EditorState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <map>
#include <string>

/**
 * @brief Serializable data structure for DataImportWidgetState
 * 
 * This struct is designed for reflect-cpp serialization.
 * All members should be default-constructible and serializable.
 */
struct DataImportWidgetStateData {
    std::string selected_import_type;                        ///< Currently selected data type (e.g., "LineData")
    std::string last_used_directory;                         ///< Persistent directory preference for file dialogs
    std::map<std::string, std::string> format_preferences;   ///< Per-type format preferences (e.g., "LineData" -> "CSV")
    std::string instance_id;                                 ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Data Import";                ///< User-visible name
};

/**
 * @brief State class for DataImport_Widget
 * 
 * DataImportWidgetState conforms to the EditorState architecture and supports
 * the passive awareness pattern for responding to data focus changes.
 * 
 * ## Usage
 * 
 * ```cpp
 * // Create state (typically done in widget constructor)
 * auto state = std::make_shared<DataImportWidgetState>();
 * workspace_manager->registerState(state);
 * 
 * // Connect to type changes
 * connect(state.get(), &DataImportWidgetState::selectedImportTypeChanged,
 *         this, &DataImport_Widget::onImportTypeChanged);
 * 
 * // Respond to data focus (from DataFocusAware::onDataFocusChanged)
 * void onDataFocusChanged(SelectedDataKey const& key, QString const& type) {
 *     state->setSelectedImportType(type);
 * }
 * ```
 */
class DataImportWidgetState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new DataImportWidgetState
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager)
     */
    explicit DataImportWidgetState(QObject * parent = nullptr);

    ~DataImportWidgetState() override = default;

    // === EditorState Interface ===

    /**
     * @brief Get type name for factory registration
     * @return "DataImportWidget"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("DataImportWidget"); }

    /**
     * @brief Get display name for UI
     * @return User-visible display name
     */
    [[nodiscard]] QString getDisplayName() const override;

    /**
     * @brief Set display name
     * @param name New display name
     */
    void setDisplayName(QString const & name) override;

    /**
     * @brief Serialize state to JSON
     * @return JSON string
     */
    [[nodiscard]] std::string toJson() const override;

    /**
     * @brief Restore state from JSON
     * @param json JSON string
     * @return true if successful
     */
    bool fromJson(std::string const & json) override;

    // === Import Type ===

    /**
     * @brief Set the selected import data type
     * 
     * This determines which loader widget is displayed in the stacked widget.
     * Typical values: "LineData", "MaskData", "PointData", "AnalogTimeSeries", etc.
     * 
     * @param type Data type string
     */
    void setSelectedImportType(QString const & type);

    /**
     * @brief Get the selected import data type
     * @return Currently selected data type
     */
    [[nodiscard]] QString selectedImportType() const;

    // === Directory Preference ===

    /**
     * @brief Set the last used directory
     * 
     * Used to remember the user's preferred directory for file dialogs.
     * 
     * @param dir Directory path
     */
    void setLastUsedDirectory(QString const & dir);

    /**
     * @brief Get the last used directory
     * @return Last used directory path
     */
    [[nodiscard]] QString lastUsedDirectory() const;

    // === Format Preferences ===

    /**
     * @brief Set format preference for a data type
     * 
     * Remembers the user's preferred format for each data type.
     * For example, if user usually imports LineData as CSV, store that preference.
     * 
     * @param data_type Data type (e.g., "LineData")
     * @param format Format string (e.g., "CSV", "HDF5", "Binary")
     */
    void setFormatPreference(QString const & data_type, QString const & format);

    /**
     * @brief Get format preference for a data type
     * @param data_type Data type
     * @return Preferred format, or empty string if not set
     */
    [[nodiscard]] QString formatPreference(QString const & data_type) const;

signals:
    /**
     * @brief Emitted when selected import type changes
     * @param type New import type
     */
    void selectedImportTypeChanged(QString const & type);

    /**
     * @brief Emitted when last used directory changes
     * @param dir New directory path
     */
    void lastUsedDirectoryChanged(QString const & dir);

    /**
     * @brief Emitted when a format preference changes
     * @param data_type Data type that was updated
     * @param format New format preference
     */
    void formatPreferenceChanged(QString const & data_type, QString const & format);

private:
    DataImportWidgetStateData _data;
};

#endif // DATA_IMPORT_WIDGET_STATE_HPP
