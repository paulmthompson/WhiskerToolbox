#ifndef TABLE_DESIGNER_STATE_HPP
#define TABLE_DESIGNER_STATE_HPP

/**
 * @file TableDesignerState.hpp
 * @brief State class for TableDesignerWidget
 * 
 * TableDesignerState manages the serializable state for the TableDesignerWidget,
 * enabling workspace save/restore and inter-widget communication via SelectionContext.
 * 
 * ## Design Pattern
 * 
 * This class follows the same pattern as MediaWidgetState and DataViewerState:
 * - Inherits from EditorState for common functionality
 * - Uses TableDesignerStateData for reflect-cpp serialization
 * - Emits consolidated Qt signals for state changes
 * 
 * ## State Categories
 * 
 * | Category | Description | Example Properties |
 * |----------|-------------|--------------------|
 * | Table Selection | Currently selected table | current_table_id |
 * | Row Settings | Row source configuration | source_name, capture_range |
 * | Group Mode | Data source grouping | enabled, pattern |
 * | Computer States | Column/computer settings | enabled state, custom names |
 * | Column Order | Preview column ordering | per-table column order |
 * 
 * ## Usage
 * 
 * ```cpp
 * // Create state
 * auto state = std::make_shared<TableDesignerState>();
 * 
 * // === Table Selection ===
 * state->setCurrentTableId("table_1");
 * 
 * // === Row Settings ===
 * state->setRowSourceName("Intervals: trials");
 * state->setCaptureRange(15000);
 * state->setIntervalMode(IntervalRowMode::Beginning);
 * 
 * // === Group Mode ===
 * state->setGroupModeEnabled(true);
 * state->setGroupingPattern("(.+)_\\d+$");
 * 
 * // === Computer States ===
 * state->setComputerEnabled("analog:signal||Mean", true);
 * state->setComputerColumnName("analog:signal||Mean", "Signal_Mean");
 * 
 * // === Serialization ===
 * std::string json = state->toJson();
 * state->fromJson(json);
 * ```
 * 
 * @see EditorState for base class documentation
 * @see TableDesignerStateData for the complete state structure
 * @see TableDesignerWidget for the widget implementation
 */

#include "EditorState/EditorState.hpp"
#include "TableDesignerStateData.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <QStringList>

#include <string>
#include <utility>
#include <vector>

/**
 * @brief State class for TableDesignerWidget
 * 
 * TableDesignerState is the Qt wrapper around TableDesignerStateData that provides
 * typed accessors and Qt signals for all state properties.
 * 
 * ## Signal Categories
 * 
 * - **Table Selection**: currentTableIdChanged
 * - **Row Settings**: rowSettingsChanged
 * - **Group Mode**: groupSettingsChanged
 * - **Computer States**: computerStateChanged
 * - **Column Order**: columnOrderChanged
 * 
 * ## Thread Safety
 * 
 * This class is NOT thread-safe. All access should be from the main/GUI thread.
 */
class TableDesignerState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new TableDesignerState
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager)
     */
    explicit TableDesignerState(QObject * parent = nullptr);

    ~TableDesignerState() override = default;

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "TableDesigner"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("TableDesigner"); }

    /**
     * @brief Get the display name for UI
     * @return User-visible name (default: "Table Designer")
     */
    [[nodiscard]] QString getDisplayName() const override;

    /**
     * @brief Set the display name
     * @param name New display name
     */
    void setDisplayName(QString const & name) override;

    // === Serialization ===

    /**
     * @brief Serialize state to JSON
     * @return JSON string representation
     */
    [[nodiscard]] std::string toJson() const override;

    /**
     * @brief Restore state from JSON
     * @param json JSON string to parse
     * @return true if parsing succeeded
     */
    bool fromJson(std::string const & json) override;

    // === Direct Data Access ===

    /**
     * @brief Get const reference to underlying data for efficiency
     * 
     * Use this for reading multiple values without individual accessor overhead.
     * For modification, use the typed setters to ensure signals are emitted.
     * 
     * @return Const reference to TableDesignerStateData
     */
    [[nodiscard]] TableDesignerStateData const & data() const { return _data; }

    // ==================== Table Selection ====================

    /**
     * @brief Set the currently selected table ID
     * @param table_id Table ID (empty string to clear selection)
     */
    void setCurrentTableId(QString const & table_id);

    /**
     * @brief Get the currently selected table ID
     * @return Current table ID, or empty string if none selected
     */
    [[nodiscard]] QString currentTableId() const { return QString::fromStdString(_data.current_table_id); }

    // ==================== Row Settings ====================

    /**
     * @brief Set the row source name
     * @param source_name Row source (e.g., "Intervals: trials", "Events: licks")
     */
    void setRowSourceName(QString const & source_name);

    /**
     * @brief Get the row source name
     * @return Current row source name
     */
    [[nodiscard]] QString rowSourceName() const { return QString::fromStdString(_data.row_settings.source_name); }

    /**
     * @brief Set the capture range for interval mode
     * @param range Capture range in samples
     */
    void setCaptureRange(int range);

    /**
     * @brief Get the capture range
     * @return Current capture range in samples
     */
    [[nodiscard]] int captureRange() const { return _data.row_settings.capture_range; }

    /**
     * @brief Set the interval row mode
     * @param mode How intervals are used (Beginning, End, or Itself)
     */
    void setIntervalMode(IntervalRowMode mode);

    /**
     * @brief Get the interval row mode
     * @return Current interval mode
     */
    [[nodiscard]] IntervalRowMode intervalMode() const { return _data.row_settings.interval_mode; }

    /**
     * @brief Set the complete row settings
     * @param settings New row settings
     */
    void setRowSettings(RowSourceSettings const & settings);

    /**
     * @brief Get the complete row settings
     * @return Const reference to RowSourceSettings
     */
    [[nodiscard]] RowSourceSettings const & rowSettings() const { return _data.row_settings; }

    // ==================== Group Mode Settings ====================

    /**
     * @brief Enable or disable group mode
     * @param enabled Whether group mode should be active
     */
    void setGroupModeEnabled(bool enabled);

    /**
     * @brief Check if group mode is enabled
     * @return true if group mode is active
     */
    [[nodiscard]] bool groupModeEnabled() const { return _data.group_settings.enabled; }

    /**
     * @brief Set the grouping regex pattern
     * @param pattern Regex pattern for grouping data sources
     */
    void setGroupingPattern(QString const & pattern);

    /**
     * @brief Get the grouping regex pattern
     * @return Current grouping pattern
     */
    [[nodiscard]] QString groupingPattern() const { return QString::fromStdString(_data.group_settings.pattern); }

    /**
     * @brief Set the complete group mode settings
     * @param settings New group settings
     */
    void setGroupSettings(GroupModeSettings const & settings);

    /**
     * @brief Get the complete group mode settings
     * @return Const reference to GroupModeSettings
     */
    [[nodiscard]] GroupModeSettings const & groupSettings() const { return _data.group_settings; }

    // ==================== Computer States ====================

    /**
     * @brief Set whether a computer is enabled
     * @param key Computer key (format: "dataSource||computerName")
     * @param enabled Whether the computer is enabled
     */
    void setComputerEnabled(QString const & key, bool enabled);

    /**
     * @brief Check if a computer is enabled
     * @param key Computer key
     * @return true if enabled, false if disabled or not found
     */
    [[nodiscard]] bool isComputerEnabled(QString const & key) const;

    /**
     * @brief Set the custom column name for a computer
     * @param key Computer key (format: "dataSource||computerName")
     * @param column_name Custom column name (empty = use default)
     */
    void setComputerColumnName(QString const & key, QString const & column_name);

    /**
     * @brief Get the custom column name for a computer
     * @param key Computer key
     * @return Custom column name, or empty string if using default
     */
    [[nodiscard]] QString computerColumnName(QString const & key) const;

    /**
     * @brief Get the complete state for a computer
     * @param key Computer key
     * @return Pointer to ComputerStateEntry, or nullptr if not found
     */
    [[nodiscard]] ComputerStateEntry const * getComputerState(QString const & key) const;

    /**
     * @brief Set the complete state for a computer
     * @param key Computer key
     * @param state The computer state
     */
    void setComputerState(QString const & key, ComputerStateEntry const & state);

    /**
     * @brief Remove a computer state entry
     * @param key Computer key
     * @return true if removed, false if not found
     */
    bool removeComputerState(QString const & key);

    /**
     * @brief Clear all computer states
     */
    void clearComputerStates();

    /**
     * @brief Get all computer keys that are enabled
     * @return List of enabled computer keys
     */
    [[nodiscard]] QStringList enabledComputerKeys() const;

    /**
     * @brief Get all computer states as a map
     * @return Const reference to the computer states map
     */
    [[nodiscard]] std::map<std::string, ComputerStateEntry> const & computerStates() const { return _data.computer_states; }

    // ==================== Column Order ====================

    /**
     * @brief Set the column order for a table's preview
     * @param table_id Table ID
     * @param column_names Ordered list of column names
     */
    void setColumnOrder(QString const & table_id, QStringList const & column_names);

    /**
     * @brief Get the column order for a table's preview
     * @param table_id Table ID
     * @return Ordered list of column names, or empty list if not set
     */
    [[nodiscard]] QStringList columnOrder(QString const & table_id) const;

    /**
     * @brief Remove the column order for a table
     * @param table_id Table ID
     * @return true if removed, false if not found
     */
    bool removeColumnOrder(QString const & table_id);

    /**
     * @brief Clear all column orders
     */
    void clearColumnOrders();

signals:
    // === Consolidated Signals ===

    /**
     * @brief Emitted when the current table ID changes
     * @param table_id New table ID
     */
    void currentTableIdChanged(QString const & table_id);

    /**
     * @brief Emitted when any row setting changes
     */
    void rowSettingsChanged();

    /**
     * @brief Emitted when group mode settings change
     */
    void groupSettingsChanged();

    /**
     * @brief Emitted when a computer state changes
     * @param key The computer key that changed
     */
    void computerStateChanged(QString const & key);

    /**
     * @brief Emitted when all computer states are cleared
     */
    void computerStatesCleared();

    /**
     * @brief Emitted when column order changes for a table
     * @param table_id The table ID whose order changed
     */
    void columnOrderChanged(QString const & table_id);

private:
    TableDesignerStateData _data;
};

#endif // TABLE_DESIGNER_STATE_HPP
