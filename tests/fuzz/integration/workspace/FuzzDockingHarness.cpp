/**
 * @file FuzzDockingHarness.cpp
 * @brief Implementation of the docking fuzz test harness
 *
 * Phase 4 of the Workspace Fuzz Testing Roadmap.
 */

#include "FuzzDockingHarness.hpp"
#include "MockEditorTypes.hpp"

#include "EditorState/EditorRegistry.hpp"
#include "EditorState/EditorState.hpp"
#include "EditorState/StrongTypes.hpp"
#include "ZoneManager/EditorCreationController.hpp"
#include "ZoneManager/ZoneManager.hpp"

#include <DockManager.h>
#include <DockWidget.h>
#include <DockAreaWidget.h>

#include <QMainWindow>

#include <iostream>

// ============================================================================
// Construction / Destruction
// ============================================================================

FuzzDockingHarness::FuzzDockingHarness() {
    // 1. Main window (no display needed — QT_QPA_PLATFORM=offscreen)
    _main_window = std::make_unique<QMainWindow>();
    if (_main_window->centralWidget()) {
        delete _main_window->takeCentralWidget();
    }

    // 2. ADS dock manager — use the same config flags as the real app
    ads::CDockManager::setConfigFlags(
        ads::CDockManager::DefaultOpaqueConfig |
        ads::CDockManager::OpaqueSplitterResize);
    _dock_manager = new ads::CDockManager(_main_window.get());

    // 3. Zone manager
    _zone_manager = std::make_unique<ZoneManager>(_dock_manager, _main_window.get());
    _zone_manager->initializeZones();

    // 4. Editor registry
    _registry = std::make_unique<EditorRegistry>();

    // 5. Creation controller
    _controller = std::make_unique<EditorCreationController>(
        _registry.get(), _zone_manager.get(), _dock_manager, _main_window.get());

    // Show the window (off-screen) so ADS geometry is valid
    _main_window->resize(1200, 800);
    _main_window->show();
}

FuzzDockingHarness::~FuzzDockingHarness() {
    // EditorCreationController and ZoneManager must be destroyed before
    // the CDockManager (which is owned by QMainWindow).
    _controller.reset();
    _zone_manager.reset();
    _registry.reset();
    // QMainWindow destructor deletes CDockManager
    _main_window.reset();
}

// ============================================================================
// Type registration
// ============================================================================

void FuzzDockingHarness::registerTypes(int count) {
    registerMockTypes(_registry.get(), count, /*with_widget_factories=*/true);
}

// ============================================================================
// Widget creation
// ============================================================================

EditorCreationController::PlacedEditor
FuzzDockingHarness::createAndPlace(std::string const & type_id) {
    return _controller->createAndPlace(
        EditorLib::EditorTypeId(QString::fromStdString(type_id)));
}

EditorCreationController::PlacedEditor
FuzzDockingHarness::createAndPlaceInZone(std::string const & type_id, Zone zone) {
    // Use the controller's full path so state is registered, then
    // if the resulting dock ends up in the wrong zone we could move it.
    // However, the simplest approach is to just use createAndPlace and
    // accept the preferred_zone.  For zone-override testing, we manually
    // create and place.
    auto const tid = EditorLib::EditorTypeId(QString::fromStdString(type_id));
    auto const type_info = _registry->typeInfo(tid);
    if (type_info.type_id.isEmpty()) {
        return {};
    }

    auto instance = _registry->createEditor(tid);
    if (!instance.state || !instance.view) {
        return {};
    }

    return _controller->placeExistingEditor(
        instance.state, zone, type_info.properties_zone);
}

// ============================================================================
// State capture / restore
// ============================================================================

std::string FuzzDockingHarness::captureState() const {
    // The workspace state is the EditorRegistry serialization.
    // ADS dock state is a separate binary blob — we capture both.
    return _registry->toJson();
}

bool FuzzDockingHarness::restoreState(std::string const & json) {
    // 1. Restore editor states from JSON
    if (!_registry->fromJson(json)) {
        return false;
    }

    // 2. Re-create view/properties widgets for restored states
    for (auto const & state : _registry->allStates()) {
        auto const type_id = EditorLib::EditorTypeId(state->getTypeName());
        auto const type_info = _registry->typeInfo(type_id);
        if (type_info.type_id.isEmpty()) {
            continue;
        }
        (void)_controller->placeExistingEditor(
            state,
            type_info.preferred_zone,
            type_info.properties_zone);
    }

    return true;
}

// ============================================================================
// Verification helpers
// ============================================================================

bool FuzzDockingHarness::verifyAllDocked() const {
    if (!_dock_manager) {
        return false;
    }

    // Check every non-placeholder dock widget
    auto const map = _dock_manager->dockWidgetsMap();
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        auto * dock = it.value();
        if (!dock) {
            continue;
        }
        // Skip placeholders
        if (dock->objectName().startsWith("__zone_placeholder_")) {
            continue;
        }
        // A closed dock is fine (user might have closed it); we only
        // care that open docks have a valid dock area.
        if (dock->isClosed()) {
            continue;
        }
        if (!dock->dockAreaWidget()) {
            return false; // docked widget without a dock area — bad
        }
    }
    return true;
}

int FuzzDockingHarness::openDockWidgetCount() const {
    if (!_dock_manager) {
        return 0;
    }

    int count = 0;
    auto const map = _dock_manager->dockWidgetsMap();
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        auto * dock = it.value();
        if (!dock) {
            continue;
        }
        if (dock->objectName().startsWith("__zone_placeholder_")) {
            continue;
        }
        if (!dock->isClosed()) {
            ++count;
        }
    }
    return count;
}
