#ifndef ZONE_MANAGER_HPP
#define ZONE_MANAGER_HPP

/**
 * @file ZoneManager.hpp
 * @brief Manages standard UI zones (dock areas) for consistent widget placement
 * 
 * ZoneManager provides a standardized way to place widgets into predictable
 * UI zones, following the architecture:
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

#include <QObject>
#include <QString>

#include <map>

// Forward declarations
namespace ads {
class CDockManager;
class CDockWidget;
class CDockAreaWidget;
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

private:
    ads::CDockManager * _dock_manager;
    bool _zones_initialized = false;

    // Zone dock areas (one per zone)
    std::map<Zone, ads::CDockAreaWidget *> _zone_areas;

    // Placeholder dock widgets (used to establish zone areas)
    std::map<Zone, ads::CDockWidget *> _placeholder_docks;

    // Zone size ratios
    float _left_ratio = 0.15f;
    float _center_ratio = 0.70f;
    float _right_ratio = 0.15f;
    float _bottom_ratio = 0.10f;

    /**
     * @brief Create a placeholder dock widget for a zone
     */
    ads::CDockWidget * createPlaceholderDock(Zone zone);

    /**
     * @brief Apply splitter sizes based on current ratios
     */
    void applySplitterSizes();
};

#endif  // ZONE_MANAGER_HPP
