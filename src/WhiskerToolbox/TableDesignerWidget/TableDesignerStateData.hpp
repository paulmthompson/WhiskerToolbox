#ifndef TABLE_DESIGNER_STATE_DATA_HPP
#define TABLE_DESIGNER_STATE_DATA_HPP

/**
 * @file TableDesignerStateData.hpp
 * @brief Comprehensive serializable state data structure for TableDesignerWidget
 * 
 * This file defines the full state structure that TableDesignerState serializes to JSON
 * using reflect-cpp. It captures all persistent state needed for workspace save/restore:
 * 
 * - Currently selected table ID
 * - Row source selection and settings (capture range, interval mode)
 * - Column/computer enabled states and custom names
 * - Group mode settings
 * - Preview column order
 * 
 * ## Design Principles
 * 
 * 1. **Nested objects for clarity** - Top-level structure uses nested objects for
 *    clear JSON organization
 * 2. **Native enum serialization** - Enums serialize as strings automatically
 * 3. **No Qt types** - All Qt types converted to std equivalents (QString → std::string)
 * 4. **Transient state excluded** - Parameter widgets, tree widget state, etc. NOT included
 * 
 * ## Example JSON Output
 * 
 * ```json
 * {
 *   "instance_id": "abc123",
 *   "display_name": "Table Designer",
 *   "current_table_id": "table_1",
 *   "row_settings": {
 *     "source_name": "Intervals: trial_intervals",
 *     "capture_range": 30000,
 *     "interval_mode": "Beginning"
 *   },
 *   "group_settings": {
 *     "enabled": true,
 *     "pattern": "(.+)_\\d+$"
 *   },
 *   "computer_states": {
 *     "analog:signal_1||Mean": {
 *       "enabled": true,
 *       "column_name": "Signal1_Mean"
 *     }
 *   }
 * }
 * ```
 * 
 * @see TableDesignerState for the Qt wrapper class
 * @see TableDesignerWidget for the widget implementation
 */

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <map>
#include <string>
#include <vector>

// ==================== Interval Mode ====================

/**
 * @brief Mode for how intervals are used as row sources
 * 
 * Serializes as "Beginning", "End", or "Itself" automatically.
 */
enum class IntervalRowMode {
    Beginning,  ///< Use beginning of interval with capture range
    End,        ///< Use end of interval with capture range
    Itself      ///< Use the interval as-is (no capture range)
};

// ==================== Row Settings ====================

/**
 * @brief Settings for row source selection
 */
struct RowSourceSettings {
    std::string source_name;                         ///< Selected row source (e.g., "Intervals: trial_intervals")
    int capture_range = 30000;                       ///< Capture range in samples (for interval mode)
    IntervalRowMode interval_mode = IntervalRowMode::Beginning;  ///< How intervals are used
};

// ==================== Group Mode Settings ====================

/**
 * @brief Settings for group mode organization
 */
struct GroupModeSettings {
    bool enabled = true;                             ///< Whether group mode is active
    std::string pattern = "(.+)_\\d+$";              ///< Regex pattern for grouping data sources
};

// ==================== Computer State ====================

/**
 * @brief State for a single computer/column in the tree
 */
struct ComputerStateEntry {
    bool enabled = false;                            ///< Whether this computer is checked/enabled
    std::string column_name;                         ///< Custom column name (empty = use default)
};

// ==================== Column Order ====================

/**
 * @brief Column order for preview per table
 */
struct TableColumnOrder {
    std::string table_id;                            ///< Table ID this order applies to
    std::vector<std::string> column_names;           ///< Ordered list of column names
};

// ==================== Main State Structure ====================

/**
 * @brief Complete serializable state for TableDesignerWidget
 * 
 * This struct contains all persistent state that should be saved/restored
 * when serializing a workspace. Transient state (parameter widgets, tree
 * widget items, etc.) is intentionally excluded.
 * 
 * ## State Categories
 * 
 * | Category | Serialized | Examples |
 * |----------|------------|----------|
 * | Table Selection | ✅ Yes | current_table_id |
 * | Row Settings | ✅ Yes | source_name, capture_range, interval_mode |
 * | Group Settings | ✅ Yes | enabled, pattern |
 * | Computer States | ✅ Yes | enabled, custom column names |
 * | Column Order | ✅ Yes | per-table preview column order |
 * | Transient State | ❌ No | Tree items, parameter widgets |
 * 
 * ## Usage
 * 
 * ```cpp
 * TableDesignerStateData data;
 * data.current_table_id = "table_1";
 * data.row_settings.source_name = "Intervals: trials";
 * data.row_settings.capture_range = 15000;
 * 
 * // Add computer state
 * ComputerStateEntry entry;
 * entry.enabled = true;
 * entry.column_name = "MyColumn";
 * data.computer_states["analog:signal||Mean"] = entry;
 * 
 * // Serialize to JSON
 * auto json = rfl::json::write(data);
 * 
 * // Deserialize from JSON
 * auto result = rfl::json::read<TableDesignerStateData>(json);
 * ```
 */
struct TableDesignerStateData {
    // === Identity ===
    std::string instance_id;                         ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Table Designer";     ///< User-visible name for this widget
    
    // === Table Selection ===
    std::string current_table_id;                    ///< Currently selected table ID (empty = none selected)
    
    // === Row Source Settings ===
    RowSourceSettings row_settings;                  ///< Row source and interval settings
    
    // === Group Mode Settings ===
    GroupModeSettings group_settings;                ///< Group mode configuration
    
    // === Computer States ===
    // Key format: "dataSource||computerName" (e.g., "analog:signal_1||Mean")
    // Value: enabled state and custom column name
    std::map<std::string, ComputerStateEntry> computer_states;
    
    // === Column Order ===
    // Per-table column order for preview
    std::map<std::string, std::vector<std::string>> column_orders;
};

#endif // TABLE_DESIGNER_STATE_DATA_HPP
