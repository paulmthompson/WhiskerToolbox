#ifndef ZONE_MANAGER_WIDGET_REGISTRATION_HPP
#define ZONE_MANAGER_WIDGET_REGISTRATION_HPP

/**
 * @file ZoneManagerWidgetRegistration.hpp
 * @brief Registration for ZoneManagerWidget with EditorRegistry
 * 
 * Provides factory registration for the ZoneManagerWidget, enabling
 * it to be created through the standard editor creation pipeline.
 */

class EditorRegistry;
class ZoneManager;

namespace ZoneManagerWidgetRegistration {

/**
 * @brief Register ZoneManagerWidget type with the EditorRegistry
 * 
 * @param registry The EditorRegistry to register with
 * @param zone_manager The ZoneManager instance to pass to created widgets
 */
void registerType(EditorRegistry * registry, ZoneManager * zone_manager);

}  // namespace ZoneManagerWidgetRegistration

#endif  // ZONE_MANAGER_WIDGET_REGISTRATION_HPP
