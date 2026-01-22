#ifndef ZONE_TYPES_HPP
#define ZONE_TYPES_HPP

/**
 * @file ZoneTypes.hpp
 * @brief Shared Zone enum for UI widget placement
 * 
 * This header defines the Zone enum used throughout the EditorState
 * and UI layout systems. It's a shared dependency to avoid circular
 * includes between EditorRegistry and ZoneManager.
 */

#include <QString>

/**
 * @brief Standard UI zones for widget placement
 * 
 * Zone represents the logical areas in the application's docking layout:
 * 
 * ```
 * ┌──────────────────────────────────────────────────────────────────┐
 * │  Menu Bar                                                        │
 * ├────────────────┬─────────────────────────────┬───────────────────┤
 * │                │                             │                   │
 * │   Left         │     Center                  │   Right           │
 * │                │                             │                   │
 * │   - Data       │     Media_Widget            │   - Properties    │
 * │     Manager    │     DataViewer_Widget       │   - Data          │
 * │   - Group      │     Analysis plots          │     Transforms    │
 * │     Manager    │     etc.                    │                   │
 * │                │                             │                   │
 * ├────────────────┴─────────────────────────────┴───────────────────┤
 * │  Bottom (Timeline, Terminal)                                     │
 * └──────────────────────────────────────────────────────────────────┘
 * ```
 */
enum class Zone {
    Left,      ///< Left panel - outliner, data tree, group management
    Center,    ///< Center - primary editing/visualization area
    Right,     ///< Right panel - properties, context-sensitive controls
    Bottom     ///< Bottom - timeline, terminal, output
};

/**
 * @brief Converts zone string to Zone enum
 * @param zone_str String representation ("left", "center", "right", "bottom", "main")
 * @return Corresponding Zone enum, defaults to Center for unknown strings
 */
inline Zone zoneFromString(QString const & zone_str) {
    QString const lower = zone_str.toLower();
    if (lower == QStringLiteral("left")) return Zone::Left;
    if (lower == QStringLiteral("right")) return Zone::Right;
    if (lower == QStringLiteral("bottom")) return Zone::Bottom;
    // "center", "main", or unknown defaults to Center
    return Zone::Center;
}

/**
 * @brief Converts Zone enum to string
 * @param zone Zone enum value
 * @return String representation
 */
inline QString zoneToString(Zone zone) {
    switch (zone) {
    case Zone::Left:   return QStringLiteral("left");
    case Zone::Center: return QStringLiteral("center");
    case Zone::Right:  return QStringLiteral("right");
    case Zone::Bottom: return QStringLiteral("bottom");
    }
    return QStringLiteral("center");  // Default
}

#endif  // ZONE_TYPES_HPP
