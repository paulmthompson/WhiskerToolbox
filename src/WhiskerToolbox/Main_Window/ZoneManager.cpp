#include "ZoneManager.hpp"

#include "DockManager.h"
#include "DockAreaWidget.h"
#include "DockWidget.h"
#include "DockSplitter.h"

#include <QLabel>
#include <QVBoxLayout>

#include <iostream>

// Note: Zone enum and zoneFromString/zoneToString functions are now
// defined as inline functions in EditorState/ZoneTypes.hpp

// === ZoneManager Implementation ===

ZoneManager::ZoneManager(ads::CDockManager * dock_manager, QObject * parent)
    : QObject(parent)
    , _dock_manager(dock_manager)
{
}

void ZoneManager::initializeZones() {
    if (_zones_initialized || !_dock_manager) {
        return;
    }

    // Create placeholder docks for each zone
    // We create them in specific order to establish the layout

    // 1. Create center zone first (this becomes the "main" area)
    auto * center_dock = createPlaceholderDock(Zone::Center);
    auto * center_area = _dock_manager->addDockWidget(ads::CenterDockWidgetArea, center_dock);
    _zone_areas[Zone::Center] = center_area;

    // 2. Create left zone to the left of center
    auto * left_dock = createPlaceholderDock(Zone::Left);
    auto * left_area = _dock_manager->addDockWidget(ads::LeftDockWidgetArea, left_dock, center_area);
    _zone_areas[Zone::Left] = left_area;

    // 3. Create right zone to the right of center
    auto * right_dock = createPlaceholderDock(Zone::Right);
    auto * right_area = _dock_manager->addDockWidget(ads::RightDockWidgetArea, right_dock, center_area);
    _zone_areas[Zone::Right] = right_area;

    // 4. Create bottom zone at the bottom (spanning all columns)
    auto * bottom_dock = createPlaceholderDock(Zone::Bottom);
    auto * bottom_area = _dock_manager->addDockWidget(ads::BottomDockWidgetArea, bottom_dock);
    _zone_areas[Zone::Bottom] = bottom_area;

    // Apply initial sizes
    applySplitterSizes();

    _zones_initialized = true;

    emit zonesReady();
}

ads::CDockAreaWidget * ZoneManager::getZoneArea(Zone zone) const {
    auto it = _zone_areas.find(zone);
    if (it != _zone_areas.end()) {
        return it->second;
    }
    return nullptr;
}

void ZoneManager::addToZone(ads::CDockWidget * dock_widget, Zone zone, bool raise) {
    if (!_zones_initialized) {
        std::cerr << "ZoneManager::addToZone: Zones not initialized!" << std::endl;
        return;
    }

    auto * zone_area = getZoneArea(zone);
    if (!zone_area) {
        std::cerr << "ZoneManager::addToZone: Zone area not found for zone: " 
                  << zoneToString(zone).toStdString() << std::endl;
        return;
    }

    // Close and hide the placeholder if it's still there
    auto placeholder_it = _placeholder_docks.find(zone);
    if (placeholder_it != _placeholder_docks.end() && placeholder_it->second) {
        auto * placeholder = placeholder_it->second;
        // Only close if this is the first real widget being added
        if (placeholder->isClosed() == false) {
            placeholder->closeDockWidget();
        }
    }

    // Add the dock widget to the zone area (as a tab)
    _dock_manager->addDockWidget(ads::CenterDockWidgetArea, dock_widget, zone_area);

    if (raise) {
        dock_widget->raise();
    }

    emit widgetAddedToZone(dock_widget, zone);
}

void ZoneManager::addBelowInZone(ads::CDockWidget * dock_widget, Zone zone, float size_ratio) {
    if (!_zones_initialized) {
        std::cerr << "ZoneManager::addBelowInZone: Zones not initialized!" << std::endl;
        return;
    }

    auto * zone_area = getZoneArea(zone);
    if (!zone_area) {
        std::cerr << "ZoneManager::addBelowInZone: Zone area not found for zone: " 
                  << zoneToString(zone).toStdString() << std::endl;
        return;
    }

    // Add below existing content
    _dock_manager->addDockWidget(ads::BottomDockWidgetArea, dock_widget, zone_area);

    // Adjust splitter sizes
    auto * splitter = ads::internal::findParent<ads::CDockSplitter *>(dock_widget->widget());
    if (splitter && splitter->count() >= 2) {
        int const total_height = splitter->height();
        int const top_height = static_cast<int>(total_height * size_ratio);
        int const bottom_height = total_height - top_height;
        splitter->setSizes({top_height, bottom_height});
    }

    emit widgetAddedToZone(dock_widget, zone);
}

Zone ZoneManager::getDefaultZone(QString const & editor_type) const {
    // Map editor types to their default zones
    // View components go to Center, Properties go to Right
    
    QString const lower = editor_type.toLower();
    
    // Left zone: data management, navigation
    if (lower.contains("datamanager") || 
        lower.contains("groupmanage") ||
        lower.contains("outliner")) {
        return Zone::Left;
    }
    
    // Right zone: properties, settings
    if (lower.contains("properties") || 
        lower.contains("inspector") ||
        lower.contains("settings")) {
        return Zone::Right;
    }
    
    // Bottom zone: time-based, output
    if (lower.contains("timeline") || 
        lower.contains("scrollbar") ||
        lower.contains("terminal") ||
        lower.contains("output")) {
        return Zone::Bottom;
    }
    
    // Default: Center zone for primary editors
    return Zone::Center;
}

void ZoneManager::setZoneWidthRatios(float left_ratio, float center_ratio, float right_ratio) {
    _left_ratio = left_ratio;
    _center_ratio = center_ratio;
    _right_ratio = right_ratio;
    
    if (_zones_initialized) {
        applySplitterSizes();
    }
}

void ZoneManager::setBottomHeightRatio(float height_ratio) {
    _bottom_ratio = height_ratio;
    
    if (_zones_initialized) {
        applySplitterSizes();
    }
}

ads::CDockWidget * ZoneManager::createPlaceholderDock(Zone zone) {
    QString const zone_name = zoneToString(zone);
    QString const dock_name = QStringLiteral("__zone_placeholder_%1").arg(zone_name);
    
    auto * dock_widget = new ads::CDockWidget(dock_name);
    
    // Create a simple placeholder widget
    auto * placeholder = new QWidget();
    auto * layout = new QVBoxLayout(placeholder);
    layout->setContentsMargins(0, 0, 0, 0);
    
    auto * label = new QLabel(QStringLiteral("Zone: %1").arg(zone_name));
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(QStringLiteral("color: #888; font-style: italic;"));
    layout->addWidget(label);
    
    dock_widget->setWidget(placeholder);
    
    // Make placeholders closable and small
    dock_widget->setFeature(ads::CDockWidget::DockWidgetClosable, true);
    dock_widget->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, false);
    dock_widget->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromContent);
    
    // Store reference for later removal
    _placeholder_docks[zone] = dock_widget;
    
    return dock_widget;
}

void ZoneManager::applySplitterSizes() {
    if (!_dock_manager) {
        return;
    }

    // Find the main horizontal splitter (contains left, center, right)
    // This is complex with ADS, we need to find the appropriate splitters
    
    auto * left_area = getZoneArea(Zone::Left);
    auto * center_area = getZoneArea(Zone::Center);
    auto * right_area = getZoneArea(Zone::Right);
    auto * bottom_area = getZoneArea(Zone::Bottom);
    
    // Try to find horizontal splitter containing the zones
    if (center_area && center_area->dockContainer()) {
        // Find the splitter that contains our areas
        auto * splitter = ads::internal::findParent<ads::CDockSplitter *>(center_area);
        if (splitter && splitter->orientation() == Qt::Horizontal) {
            int const total_width = splitter->width();
            int const left_width = static_cast<int>(total_width * _left_ratio);
            int const center_width = static_cast<int>(total_width * _center_ratio);
            int const right_width = total_width - left_width - center_width;
            
            if (splitter->count() == 3) {
                splitter->setSizes({left_width, center_width, right_width});
            }
        }
    }
    
    // Handle bottom zone vertical split
    if (bottom_area) {
        auto * vsplitter = ads::internal::findParent<ads::CDockSplitter *>(bottom_area);
        if (vsplitter && vsplitter->orientation() == Qt::Vertical) {
            int const total_height = vsplitter->height();
            int const main_height = static_cast<int>(total_height * (1.0f - _bottom_ratio));
            int const bottom_height = total_height - main_height;
            
            if (vsplitter->count() == 2) {
                vsplitter->setSizes({main_height, bottom_height});
            }
        }
    }
}
