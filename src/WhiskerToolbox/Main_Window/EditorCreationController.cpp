#include "EditorCreationController.hpp"

#include "EditorState/EditorRegistry.hpp"
#include "EditorState/EditorState.hpp"
#include "ZoneManager.hpp"

#include <DockWidget.h>
#include <DockManager.h>

#include <iostream>

EditorCreationController::EditorCreationController(EditorRegistry * registry,
                                                     ZoneManager * zone_manager,
                                                     ads::CDockManager * dock_manager,
                                                     QObject * parent)
    : QObject(parent)
    , _registry(registry)
    , _zone_manager(zone_manager)
    , _dock_manager(dock_manager)
{
    Q_ASSERT(registry != nullptr);
    Q_ASSERT(zone_manager != nullptr);
    Q_ASSERT(dock_manager != nullptr);
}

EditorCreationController::PlacedEditor 
EditorCreationController::createAndPlace(EditorLib::EditorTypeId const & type_id,
                                          bool raise_view)
{
    // Get type info to determine zone preferences
    auto const type_info = _registry->typeInfo(type_id);
    if (type_info.type_id.isEmpty()) {
        std::cerr << "EditorCreationController: Unknown type_id: " 
                  << type_id.toString().toStdString() << std::endl;
        return {};
    }
    
    // Generate unique title based on display name
    QString const view_title = generateUniqueTitle(type_info.display_name, type_id);
    
    return createAndPlaceWithTitle(type_id, view_title, raise_view);
}

EditorCreationController::PlacedEditor 
EditorCreationController::createAndPlaceWithTitle(EditorLib::EditorTypeId const & type_id,
                                                   QString const & view_title,
                                                   bool raise_view)
{
    PlacedEditor result;
    
    // Get type info for zone preferences
    auto const type_info = _registry->typeInfo(type_id);
    if (type_info.type_id.isEmpty()) {
        std::cerr << "EditorCreationController: Unknown type_id: " 
                  << type_id.toString().toStdString() << std::endl;
        return result;
    }
    
    // Create the editor instance via registry
    auto editor_instance = _registry->createEditor(type_id);
    if (!editor_instance.state) {
        std::cerr << "EditorCreationController: Failed to create editor of type: "
                  << type_id.toString().toStdString() << std::endl;
        return result;
    }
    
    result.state = editor_instance.state;
    
    // Increment creation counter for this type
    _creation_counters[type_id]++;
    
    // Create and place the view dock widget
    if (editor_instance.view) {
        result.view_dock = createDockWidget(editor_instance.view, view_title, true);
        
        if (result.view_dock) {
            // Add to the preferred zone
            _zone_manager->addToZone(result.view_dock, type_info.preferred_zone, raise_view);
        }
    }
    
    // Create and place the properties dock widget (if properties factory exists)
    if (editor_instance.properties) {
        QString const props_title = view_title + QStringLiteral(" Properties");
        result.properties_dock = createDockWidget(editor_instance.properties, props_title, true);
        
        if (result.properties_dock) {
            // Determine whether to raise properties based on type info
            bool const raise_props = type_info.auto_raise_properties;
            
            // Add to the properties zone (as tab or direct based on type_info.properties_as_tab)
            // ZoneManager::addToZone always adds as tab, which is the default behavior
            _zone_manager->addToZone(result.properties_dock, type_info.properties_zone, raise_props);
        }
    }
    
    // Connect cleanup signals
    auto const instance_id = EditorLib::EditorInstanceId(result.state->getInstanceId());
    connectCleanupSignals(result, instance_id);
    
    // Emit signal for successful placement
    emit editorPlaced(instance_id, type_id);
    
    return result;
}

int EditorCreationController::createdCount(EditorLib::EditorTypeId const & type_id) const
{
    auto const it = _creation_counters.find(type_id);
    return (it != _creation_counters.end()) ? it->second : 0;
}

ads::CDockWidget * EditorCreationController::createDockWidget(QWidget * widget,
                                                               QString const & title,
                                                               bool closable)
{
    if (!widget) {
        return nullptr;
    }
    
    auto * dock = new ads::CDockWidget(title);
    dock->setWidget(widget);
    
    // Configure dock features
    dock->setFeature(ads::CDockWidget::DockWidgetClosable, closable);
    dock->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, false);
    dock->setFeature(ads::CDockWidget::DockWidgetMovable, true);
    dock->setFeature(ads::CDockWidget::DockWidgetFloatable, true);
    
    return dock;
}

void EditorCreationController::connectCleanupSignals(PlacedEditor const & editor,
                                                      EditorLib::EditorInstanceId const & instance_id)
{
    if (!editor.view_dock) {
        return;
    }
    
    // When the view dock is closed, unregister the state from the registry
    connect(editor.view_dock, &ads::CDockWidget::closed, this, 
            [this, instance_id]() {
                if (_registry) {
                    _registry->unregisterState(instance_id);
                }
                emit editorClosed(instance_id);
            });
    
    // If there's a properties dock, close it when the view is closed
    if (editor.properties_dock) {
        ads::CDockWidget * props_dock = editor.properties_dock;
        connect(editor.view_dock, &ads::CDockWidget::closed, this,
                [props_dock]() {
                    if (props_dock) {
                        props_dock->closeDockWidget();
                    }
                });
    }
}

QString EditorCreationController::generateUniqueTitle(QString const & base_name,
                                                       EditorLib::EditorTypeId const & type_id)
{
    int const count = createdCount(type_id);
    
    // First instance doesn't need a number suffix
    if (count == 0) {
        return base_name;
    }
    
    // Subsequent instances get numbered (e.g., "Media Viewer 2", "Media Viewer 3")
    return base_name + QStringLiteral(" %1").arg(count + 1);
}
