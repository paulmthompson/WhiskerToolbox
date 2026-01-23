#ifndef ZONE_MANAGER_HPP
#define ZONE_MANAGER_HPP

/**
 * @file ZoneManager.hpp
 * @brief Manages standard UI zones (dock areas) for consistent widget placement
 * 
 * ZoneManager provides a standardized way to place widgets into predictable
 * UI zones, following the architecture.
 * 
 * ## Runtime Configuration
 * 
 * Zone layouts can be persisted to JSON and loaded at runtime:
 * ```cpp
 * // Save current layout
 * auto config = zone_manager->captureCurrentConfig();
 * ZoneConfig::saveToFile(config, "layout.json");
 * 
 * // Load and apply layout
 * auto result = ZoneConfig::loadFromFile("layout.json");
 * if (result) {
 *     zone_manager->applyConfig(*result);
 * }
 * ```
 * 
 * Enable auto-save to persist layout changes automatically:
 * ```cpp
 * zone_manager->setAutoSaveEnabled(true);
 * zone_manager->setAutoSaveFilePath("~/.config/whisker/layout.json");
 * ```
 * 
 * Layout follows this architecture:
 * 
 * ```
 * ┌──────────────────────────────────────────────────────────────────┐
 * │  Menu Bar                                                        │
 * ├────────────────┬─────────────────────────────┬───────────────────┤
 * │                │                             │                   │
 * │   Outliner     │     Main Editor Area        │   Properties      │
 * │   (Left)       │     (Center)                │   (Right)         │
 * │                │                             │                   │
 * │   - Data       │     Media_Widget            │   - Editor-       │
 * │     Manager    │     DataViewer_Widget       │     specific      │
 * │                │     Analysis plots          │     properties    │
 * │   - Group      │     Test_Widget view        │                   │
 * │     Manager    │     etc.                    │                   │
 * │                │                             │                   │
 * ├────────────────┴─────────────────────────────┴───────────────────┤
 * │  Timeline (Bottom)                                               │
 * └──────────────────────────────────────────────────────────────────┘
 * ```
 * 
 * ## Zone Responsibilities
 * 
 * | Zone        | Contents                        | Purpose                    |
 * |-------------|--------------------------------|----------------------------|
 * | **Left**    | DataManager, GroupManagement   | Data selection, navigation |
 * | **Center**  | Media, DataViewer, views       | Primary visualization      |
 * | **Right**   | Properties tabs, Data Transforms| Persistent editor settings |
 * | **Bottom**  | TimeScrollBar, Terminal        | Time navigation, output    |
 * 
 * ## Usage
 * 
 * ```cpp
 * // In MainWindow constructor
 * _zone_manager = std::make_unique<ZoneManager>(_m_DockManager);
 * 
 * // Build initial layout
 * _zone_manager->initializeZones();
 * 
 * // Add widgets to zones
 * _zone_manager->addToZone(data_manager_dock, Zone::Left);
 * _zone_manager->addToZone(media_dock, Zone::Center);
 * _zone_manager->addToZone(properties_dock, Zone::Right);
 * ```
 * 
 * @see EditorCreationController for unified editor creation with zone placement
 * @see EditorRegistry for widget type registration
 */

#include "EditorState/ZoneTypes.hpp"
#include "ZoneConfig.hpp"

#include <QObject>
#include <QString>
#include <QTimer>

#include <map>

// Forward declarations
namespace ads {
class CDockManager;
class CDockWidget;
class CDockAreaWidget;
class CDockSplitter;
}

// Zone enum and conversion functions are now in EditorState/ZoneTypes.hpp

/**
 * @brief Manages standard dock zones for consistent UI layout
 * 
 * ZoneManager wraps the ADS (Advanced Docking System) dock manager to provide
 * a higher-level abstraction for placing widgets into standardized UI zones.
 * 
 * The manager:
 * - Creates placeholder dock areas for each zone during initialization
 * - Tracks which dock area represents each zone
 * - Provides methods to add widgets to specific zones
 * - Handles default zone sizing
 */
class ZoneManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct a ZoneManager
     * @param dock_manager The ADS dock manager to wrap
     * @param parent Parent QObject (typically MainWindow)
     */
    explicit ZoneManager(ads::CDockManager * dock_manager,
                         QObject * parent = nullptr);

    ~ZoneManager() override = default;

    /**
     * @brief Initialize the zone structure
     * 
     * Creates placeholder widgets for each zone to establish the layout.
     * Call this after constructing the dock manager but before adding
     * any content widgets.
     * 
     * This sets up the basic three-column layout:
     * - Left zone (15% width)
     * - Center zone (70% width) 
     * - Right zone (15% width)
     * - Bottom zone (spanning full width, minimal height)
     */
    void initializeZones();

    /**
     * @brief Check if zones have been initialized
     */
    [[nodiscard]] bool zonesInitialized() const { return _zones_initialized; }

    /**
     * @brief Get the dock area for a specific zone
     * @param zone The zone to query
     * @return Dock area widget, or nullptr if zone not initialized
     */
    [[nodiscard]] ads::CDockAreaWidget * getZoneArea(Zone zone) const;

    /**
     * @brief Add a dock widget to a specific zone
     * 
     * The widget will be added to the appropriate dock area,
     * tabbed with existing widgets in that zone.
     * 
     * @param dock_widget The dock widget to add
     * @param zone Target zone
     * @param raise If true, make this the active tab in the zone
     */
    void addToZone(ads::CDockWidget * dock_widget, Zone zone, bool raise = true);

    /**
     * @brief Add a dock widget below existing content in a zone
     * 
     * Creates a vertical split in the zone, placing the new widget
     * below existing content. Useful for adding sub-components
     * to the left or right panels.
     * 
     * @param dock_widget The dock widget to add
     * @param zone Target zone
     * @param size_ratio Ratio of original content to new content (0.0-1.0)
     */
    void addBelowInZone(ads::CDockWidget * dock_widget, Zone zone, float size_ratio = 0.7f);

    /**
     * @brief Get the default zone for an editor type
     * 
     * Maps common editor type strings to their default zones.
     * Used when opening editors without explicit zone specification.
     * 
     * @param editor_type The editor type ID (e.g., "MediaWidget", "TestWidget")
     * @return Default zone for this editor type
     */
    [[nodiscard]] Zone getDefaultZone(QString const & editor_type) const;

    /**
     * @brief Set zone width ratios
     * 
     * Adjusts the relative widths of the left, center, and right zones.
     * Ratios should sum to approximately 1.0.
     * 
     * @param left_ratio Left zone width ratio (default 0.15)
     * @param center_ratio Center zone width ratio (default 0.70)
     * @param right_ratio Right zone width ratio (default 0.15)
     */
    void setZoneWidthRatios(float left_ratio, float center_ratio, float right_ratio);

    /**
     * @brief Set bottom zone height ratio
     * 
     * @param height_ratio Bottom zone height as fraction of total height (default 0.10)
     */
    void setBottomHeightRatio(float height_ratio);

    /**
     * @brief Get the dock manager
     */
    [[nodiscard]] ads::CDockManager * dockManager() const { return _dock_manager; }

    // ========== Runtime Configuration ==========

    /**
     * @brief Capture the current layout configuration
     * 
     * Creates a ZoneLayoutConfig that reflects the current state of the UI,
     * including zone ratios and widget placement. This can be serialized
     * to JSON for persistence.
     * 
     * @return Current layout configuration
     */
    [[nodiscard]] ZoneConfig::ZoneLayoutConfig captureCurrentConfig() const;

    /**
     * @brief Apply a layout configuration
     * 
     * Updates zone ratios from the configuration. Note that this only
     * applies size ratios - widget placement must be handled separately
     * by EditorCreationController during startup.
     * 
     * @param config Configuration to apply
     * @return true if applied successfully, false on error
     */
    bool applyConfig(ZoneConfig::ZoneLayoutConfig const & config);

    /**
     * @brief Load and apply configuration from file
     * 
     * @param file_path Path to JSON configuration file
     * @return Error message if failed, empty string on success
     */
    QString loadConfigFromFile(QString const & file_path);

    /**
     * @brief Save current configuration to file
     * 
     * @param file_path Path to output JSON file
     * @return true if saved successfully
     */
    bool saveConfigToFile(QString const & file_path) const;

    /**
     * @brief Enable/disable automatic saving of layout changes
     * 
     * When enabled, layout changes (splitter resizing) will be automatically
     * saved to the configured file path after a debounce delay.
     * 
     * @param enabled Whether to enable auto-save
     */
    void setAutoSaveEnabled(bool enabled);

    /**
     * @brief Check if auto-save is enabled
     */
    [[nodiscard]] bool isAutoSaveEnabled() const { return _auto_save_enabled; }

    /**
     * @brief Set the file path for auto-save
     * 
     * @param file_path Path where configuration will be saved
     */
    void setAutoSaveFilePath(QString const & file_path);

    /**
     * @brief Get the current auto-save file path
     */
    [[nodiscard]] QString autoSaveFilePath() const { return _auto_save_path; }

    /**
     * @brief Set the debounce delay for auto-save
     * 
     * After a layout change, the system will wait this long before saving
     * to avoid excessive writes during continuous resizing.
     * 
     * @param milliseconds Delay in milliseconds (default: 500)
     */
    void setAutoSaveDebounceMs(int milliseconds);

    /**
     * @brief Get current zone ratios
     */
    [[nodiscard]] ZoneConfig::ZoneRatios currentRatios() const;

    /**
     * @brief Force reapplication of zone ratios to splitters
     * 
     * Call this after the main window is shown and has been sized.
     * The sizes are applied via a single-shot timer to ensure the
     * layout has been fully computed.
     * 
     * @param delay_ms Delay before applying sizes (default 100ms)
     */
    void reapplySplitterSizes(int delay_ms = 100);

signals:
    /**
     * @brief Emitted when zones are initialized
     */
    void zonesReady();

    /**
     * @brief Emitted when a widget is added to a zone
     * @param dock_widget The added dock widget
     * @param zone The target zone
     */
    void widgetAddedToZone(ads::CDockWidget * dock_widget, Zone zone);

    /**
     * @brief Emitted when zone ratios change
     * 
     * This is emitted after splitter resizing is complete (debounced).
     */
    void zoneRatiosChanged();

    /**
     * @brief Emitted when configuration is loaded
     * @param file_path Path that was loaded
     */
    void configLoaded(QString const & file_path);

    /**
     * @brief Emitted when configuration is saved
     * @param file_path Path that was saved to
     */
    void configSaved(QString const & file_path);

    /**
     * @brief Emitted when configuration load fails
     * @param error_message Description of the error
     */
    void configLoadError(QString const & error_message);

private slots:
    /**
     * @brief Handle splitter movement
     */
    void onSplitterMoved(int pos, int index);

    /**
     * @brief Trigger auto-save after debounce delay
     */
    void triggerAutoSave();

private:
    ads::CDockManager * _dock_manager;
    bool _zones_initialized = false;

    // Zone dock areas (one per zone)
    std::map<Zone, ads::CDockAreaWidget *> _zone_areas;

    // Placeholder dock widgets (used to establish zone areas)
    std::map<Zone, ads::CDockWidget *> _placeholder_docks;

    // Zone size ratios
    float _left_ratio = 0.20f;
    float _center_ratio = 0.58f;
    float _right_ratio = 0.22f;
    float _bottom_ratio = 0.14f;

    // Auto-save configuration
    bool _auto_save_enabled = false;
    QString _auto_save_path;
    int _auto_save_debounce_ms = 500;
    QTimer * _auto_save_timer = nullptr;

    // Tracked splitters for ratio updates
    ads::CDockSplitter * _horizontal_splitter = nullptr;
    ads::CDockSplitter * _vertical_splitter = nullptr;

    /**
     * @brief Create a placeholder dock widget for a zone
     */
    ads::CDockWidget * createPlaceholderDock(Zone zone);

    /**
     * @brief Apply splitter sizes based on current ratios
     */
    void applySplitterSizes();

    /**
     * @brief Update internal ratios from current splitter positions
     */
    void updateRatiosFromSplitters();

    /**
     * @brief Connect to splitter signals for tracking changes
     */
    void connectSplitterSignals();
};

#endif  // ZONE_MANAGER_HPP
