#include "ZoneManager.hpp"

#include "DockManager.h"
#include "DockAreaWidget.h"
#include "DockWidget.h"
#include "DockSplitter.h"
#include "DockContainerWidget.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>

#include <iostream>

// Note: Zone enum and zoneFromString/zoneToString functions are now
// defined as inline functions in EditorState/ZoneTypes.hpp

// === ZoneManager Implementation ===

ZoneManager::ZoneManager(ads::CDockManager * dock_manager, QObject * parent)
    : QObject(parent)
    , _dock_manager(dock_manager)
{
    // Create auto-save timer
    _auto_save_timer = new QTimer(this);
    _auto_save_timer->setSingleShot(true);
    connect(_auto_save_timer, &QTimer::timeout, this, &ZoneManager::triggerAutoSave);
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

    // Connect to splitter signals for tracking changes
    connectSplitterSignals();

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

    /*
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
    */
    // === FIX START ===
    // 1. Add the new widget FIRST. 
    // This ensures the DockAreaWidget always has at least one tab and is not deleted by ADS.
    _dock_manager->addDockWidget(ads::CenterDockWidgetArea, dock_widget, zone_area);

    // 2. Remove the placeholder SECOND.
    // Now it's safe to close the placeholder because the area contains the new widget.
    auto placeholder_it = _placeholder_docks.find(zone);
    if (placeholder_it != _placeholder_docks.end() && placeholder_it->second) {
        auto * placeholder = placeholder_it->second;
        // Only close if it is currently open
        if (!placeholder->isClosed()) {
            placeholder->closeDockWidget();
        }
    }
    // === FIX END ===

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

// ============================================================================
// Runtime Configuration Implementation
// ============================================================================

ZoneConfig::ZoneLayoutConfig ZoneManager::captureCurrentConfig() const {
    ZoneConfig::ZoneLayoutConfig config;
    config.version = "1.0";
    
    // Capture current ratios
    config.zone_ratios.left = _left_ratio;
    config.zone_ratios.center = _center_ratio;
    config.zone_ratios.right = _right_ratio;
    config.zone_ratios.bottom = _bottom_ratio;
    
    // Capture widgets in each zone
    auto captureZoneContent = [this](Zone zone) -> ZoneConfig::ZoneContentConfig {
        ZoneConfig::ZoneContentConfig content;
        
        auto * zone_area = getZoneArea(zone);
        if (!zone_area) {
            return content;
        }
        
        // Get all dock widgets in this zone area
        auto const & dock_widgets = zone_area->dockWidgets();
        for (auto * dock : dock_widgets) {
            // Skip placeholders
            if (dock->objectName().startsWith("__zone_placeholder_")) {
                continue;
            }
            
            ZoneConfig::WidgetConfig widget_config;
            widget_config.type_id = dock->objectName().toStdString();
            widget_config.title = dock->windowTitle().toStdString();
            widget_config.visible = !dock->isClosed();
            widget_config.closable = dock->features().testFlag(ads::CDockWidget::DockWidgetClosable);
            
            content.widgets.push_back(widget_config);
        }
        
        // Track active tab
        if (auto * current = zone_area->currentDockWidget()) {
            int idx = 0;
            for (auto * dock : dock_widgets) {
                if (dock == current) {
                    content.active_tab_index = idx;
                    break;
                }
                ++idx;
            }
        }
        
        return content;
    };
    
    config.zones["left"] = captureZoneContent(Zone::Left);
    config.zones["center"] = captureZoneContent(Zone::Center);
    config.zones["right"] = captureZoneContent(Zone::Right);
    config.zones["bottom"] = captureZoneContent(Zone::Bottom);
    
    return config;
}

bool ZoneManager::applyConfig(ZoneConfig::ZoneLayoutConfig const & config) {
    // Validate configuration
    std::string const validation_error = config.validate();
    if (!validation_error.empty()) {
        std::cerr << "ZoneManager::applyConfig: Invalid config: " << validation_error << std::endl;
        return false;
    }
    
    // Apply zone ratios
    _left_ratio = config.zone_ratios.left;
    _center_ratio = config.zone_ratios.center;
    _right_ratio = config.zone_ratios.right;
    _bottom_ratio = config.zone_ratios.bottom;
    
    // Apply splitter sizes if zones are initialized
    if (_zones_initialized) {
        applySplitterSizes();
    }
    
    emit zoneRatiosChanged();
    return true;
}

QString ZoneManager::loadConfigFromFile(QString const & file_path) {
    auto result = ZoneConfig::loadFromFile(file_path.toStdString());
    if (!result) {
        QString const error_msg = QString::fromStdString(result.error()->what());
        emit configLoadError(error_msg);
        return error_msg;
    }
    
    if (!applyConfig(*result)) {
        QString const error_msg = QStringLiteral("Failed to apply configuration");
        emit configLoadError(error_msg);
        return error_msg;
    }
    
    emit configLoaded(file_path);
    return QString();  // Success - empty error string
}

bool ZoneManager::saveConfigToFile(QString const & file_path) const {
    auto config = captureCurrentConfig();
    bool const success = ZoneConfig::saveToFile(config, file_path.toStdString());
    
    if (success) {
        // Cast away const for signal emission (this is a notification, not a state change)
        const_cast<ZoneManager *>(this)->emit configSaved(file_path);
    }
    
    return success;
}

void ZoneManager::setAutoSaveEnabled(bool enabled) {
    _auto_save_enabled = enabled;
}

void ZoneManager::setAutoSaveFilePath(QString const & file_path) {
    _auto_save_path = file_path;
}

void ZoneManager::setAutoSaveDebounceMs(int milliseconds) {
    _auto_save_debounce_ms = std::max(100, milliseconds);  // Minimum 100ms
}

ZoneConfig::ZoneRatios ZoneManager::currentRatios() const {
    return ZoneConfig::ZoneRatios{
        .left = _left_ratio,
        .center = _center_ratio,
        .right = _right_ratio,
        .bottom = _bottom_ratio
    };
}

void ZoneManager::reapplySplitterSizes(int delay_ms) {
    if (!_zones_initialized) {
        return;
    }
    
    // Use a single-shot timer to defer the size application
    // This ensures the window layout has been fully computed
    QTimer::singleShot(delay_ms, this, [this]() {
        applySplitterSizes();
        
        // Apply a second time after a short delay to handle any layout adjustments
        QTimer::singleShot(50, this, &ZoneManager::applySplitterSizes);
    });
}

void ZoneManager::onSplitterMoved(int /*pos*/, int /*index*/) {
    // Update internal ratios from splitter positions
    updateRatiosFromSplitters();
    
    // Trigger debounced auto-save
    if (_auto_save_enabled && !_auto_save_path.isEmpty()) {
        _auto_save_timer->start(_auto_save_debounce_ms);
    }
}

void ZoneManager::triggerAutoSave() {
    if (_auto_save_enabled && !_auto_save_path.isEmpty()) {
        saveConfigToFile(_auto_save_path);
    }
    emit zoneRatiosChanged();
}

void ZoneManager::updateRatiosFromSplitters() {
    // Update horizontal ratios
    if (_horizontal_splitter && _horizontal_splitter->count() == 3) {
        QList<int> const sizes = _horizontal_splitter->sizes();
        int const total = sizes[0] + sizes[1] + sizes[2];
        if (total > 0) {
            _left_ratio = static_cast<float>(sizes[0]) / static_cast<float>(total);
            _center_ratio = static_cast<float>(sizes[1]) / static_cast<float>(total);
            _right_ratio = static_cast<float>(sizes[2]) / static_cast<float>(total);
        }
    }
    
    // Update vertical ratio (bottom)
    if (_vertical_splitter && _vertical_splitter->count() == 2) {
        QList<int> const sizes = _vertical_splitter->sizes();
        int const total = sizes[0] + sizes[1];
        if (total > 0) {
            _bottom_ratio = static_cast<float>(sizes[1]) / static_cast<float>(total);
        }
    }
}

void ZoneManager::connectSplitterSignals() {
    auto * center_area = getZoneArea(Zone::Center);
    auto * bottom_area = getZoneArea(Zone::Bottom);
    
    // Find and connect to horizontal splitter
    if (center_area) {
        _horizontal_splitter = ads::internal::findParent<ads::CDockSplitter *>(center_area);
        if (_horizontal_splitter && _horizontal_splitter->orientation() == Qt::Horizontal) {
            connect(_horizontal_splitter, &QSplitter::splitterMoved,
                    this, &ZoneManager::onSplitterMoved);
        }
    }
    
    // Find and connect to vertical splitter
    if (bottom_area) {
        _vertical_splitter = ads::internal::findParent<ads::CDockSplitter *>(bottom_area);
        if (_vertical_splitter && _vertical_splitter->orientation() == Qt::Vertical) {
            connect(_vertical_splitter, &QSplitter::splitterMoved,
                    this, &ZoneManager::onSplitterMoved);
        }
    }
}
