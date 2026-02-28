/**
 * @file FuzzDockingHarness.hpp
 * @brief Reusable test fixture for ADS docking + ZoneManager fuzz tests
 *
 * Provides a self-contained QMainWindow + CDockManager + ZoneManager +
 * EditorRegistry + EditorCreationController harness that can be
 * constructed / destroyed inside each fuzz iteration.
 *
 * Requires QApplication (use QT_QPA_PLATFORM=offscreen in CI).
 *
 * Phase 4 of the Workspace Fuzz Testing Roadmap.
 */

#ifndef FUZZ_DOCKING_HARNESS_HPP
#define FUZZ_DOCKING_HARNESS_HPP

#include "EditorState/EditorRegistry.hpp"
#include "EditorState/ZoneTypes.hpp"
#include "ZoneManager/EditorCreationController.hpp"
#include "ZoneManager/ZoneManager.hpp"

#include <QMainWindow>

#include <memory>
#include <string>
#include <vector>

namespace ads {
class CDockManager;
class CDockWidget;
}

/**
 * @brief Lightweight harness for fuzz-testing widget placement + serialization
 *
 * Creates a headless QMainWindow + ADS infrastructure identical in shape to
 * the production MainWindow, registers mock editor types with minimal QWidget
 * factories, then exposes helpers for:
 *   - creating & placing editors in random zones
 *   - capturing the full workspace state (editor JSON + ADS dock state)
 *   - restoring state into a fresh harness and verifying invariants
 */
class FuzzDockingHarness {
public:
    FuzzDockingHarness();
    ~FuzzDockingHarness();

    // Non-copyable, non-movable (owns Qt objects)
    FuzzDockingHarness(FuzzDockingHarness const &) = delete;
    FuzzDockingHarness & operator=(FuzzDockingHarness const &) = delete;

    // ---- Type registration ----

    /**
     * @brief Register mock editor types with widget factories
     * @param count How many mock types to register (1-3)
     */
    void registerTypes(int count = 3);

    // ---- Widget creation ----

    /**
     * @brief Create and place an editor of the given type
     * @param type_id Editor type ID (e.g., "MockTypeA")
     * @return The PlacedEditor result from EditorCreationController
     */
    EditorCreationController::PlacedEditor
    createAndPlace(std::string const & type_id);

    /**
     * @brief Create and place an editor, overriding the zone
     * @param type_id Editor type ID
     * @param zone Zone to place the view in (overrides preferred_zone)
     */
    EditorCreationController::PlacedEditor
    createAndPlaceInZone(std::string const & type_id, Zone zone);

    // ---- State capture / restore ----

    /**
     * @brief Serialize the full workspace (editor states + ADS dock state)
     * @return JSON string of the workspace, empty on failure
     */
    [[nodiscard]] std::string captureState() const;

    /**
     * @brief Restore workspace state from JSON
     * @param json Output of a previous captureState() call
     * @return true on success
     */
    bool restoreState(std::string const & json);

    // ---- Verification helpers ----

    /**
     * @brief Verify all non-placeholder dock widgets are docked (not floating/hidden)
     */
    [[nodiscard]] bool verifyAllDocked() const;

    /**
     * @brief Get the number of open (visible, non-placeholder) dock widgets
     */
    [[nodiscard]] int openDockWidgetCount() const;

    // ---- Accessors ----

    [[nodiscard]] EditorRegistry * registry() { return _registry.get(); }
    [[nodiscard]] ZoneManager * zoneManager() { return _zone_manager.get(); }
    [[nodiscard]] EditorCreationController * controller() { return _controller.get(); }
    [[nodiscard]] ads::CDockManager * dockManager() { return _dock_manager; }
    [[nodiscard]] QMainWindow * mainWindow() { return _main_window.get(); }

private:
    std::unique_ptr<QMainWindow> _main_window;
    ads::CDockManager * _dock_manager = nullptr;           // owned by QMainWindow
    std::unique_ptr<ZoneManager> _zone_manager;
    std::unique_ptr<EditorRegistry> _registry;
    std::unique_ptr<EditorCreationController> _controller;
};

#endif // FUZZ_DOCKING_HARNESS_HPP
