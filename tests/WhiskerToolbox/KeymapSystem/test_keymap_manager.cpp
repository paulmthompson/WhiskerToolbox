/**
 * @file test_keymap_manager.cpp
 * @brief Unit tests for KeymapManager: registration, overrides, conflict detection, and resolution
 */

#include <catch2/catch_test_macros.hpp>

#include "KeymapSystem/KeyAction.hpp"
#include "KeymapSystem/KeyActionAdapter.hpp"
#include "KeymapSystem/Keymap.hpp"
#include "KeymapSystem/KeymapManager.hpp"

#include <QCoreApplication>
#include <QKeySequence>

using namespace KeymapSystem;
using EditorLib::EditorInstanceId;
using EditorLib::EditorTypeId;

namespace {

/// Ensure QCoreApplication exists for Qt signal/slot usage
struct QtAppGuard {
    QtAppGuard() {
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char const * argv[] = {"test_keymap_system"};
            static QCoreApplication app(argc, const_cast<char **>(argv));
        }
    }
};

[[maybe_unused]] static QtAppGuard const qt_guard;

KeyActionDescriptor makeAction(
        QString id,
        QString name,
        QString category,
        KeyActionScope scope,
        QKeySequence key = {}) {
    return {std::move(id), std::move(name), std::move(category), std::move(scope), std::move(key)};
}

}// namespace

// ============================================================
// Registration
// ============================================================

TEST_CASE("registerAction stores action and allActions returns it") {
    KeymapManager mgr;

    auto desc = makeAction(
            "test.action_a", "Action A", "Test",
            KeyActionScope::global(), QKeySequence(Qt::Key_F1));

    REQUIRE(mgr.registerAction(desc));
    auto actions = mgr.allActions();
    REQUIRE(actions.size() == 1);
    CHECK(actions[0].action_id == "test.action_a");
    CHECK(actions[0].display_name == "Action A");
    CHECK(actions[0].default_binding == QKeySequence(Qt::Key_F1));
}

TEST_CASE("registerAction rejects duplicate action_id") {
    KeymapManager mgr;

    auto desc = makeAction("test.dup", "Dup", "Test", KeyActionScope::global());
    REQUIRE(mgr.registerAction(desc));
    REQUIRE_FALSE(mgr.registerAction(desc));
    CHECK(mgr.allActions().size() == 1);
}

TEST_CASE("unregisterAction removes action and its override") {
    KeymapManager mgr;

    auto desc = makeAction("test.remove", "Remove", "Test",
                           KeyActionScope::global(), QKeySequence(Qt::Key_A));
    mgr.registerAction(desc);
    mgr.setUserOverride("test.remove", QKeySequence(Qt::Key_B));

    REQUIRE(mgr.unregisterAction("test.remove"));
    CHECK(mgr.allActions().empty());
    CHECK(mgr.bindingFor("test.remove").isEmpty());
}

TEST_CASE("unregisterAction returns false for unknown action") {
    KeymapManager mgr;
    REQUIRE_FALSE(mgr.unregisterAction("nonexistent"));
}

TEST_CASE("action() returns descriptor or nullopt") {
    KeymapManager mgr;

    auto desc = makeAction("test.q", "Query", "Test", KeyActionScope::global());
    mgr.registerAction(desc);

    auto result = mgr.action("test.q");
    REQUIRE(result.has_value());
    CHECK(result->display_name == "Query");

    CHECK_FALSE(mgr.action("nonexistent").has_value());
}

// ============================================================
// Binding resolution
// ============================================================

TEST_CASE("bindingFor returns default when no override") {
    KeymapManager mgr;

    auto desc = makeAction("test.bind", "Bind", "Test",
                           KeyActionScope::global(), QKeySequence(Qt::Key_Space));
    mgr.registerAction(desc);

    CHECK(mgr.bindingFor("test.bind") == QKeySequence(Qt::Key_Space));
}

TEST_CASE("bindingFor returns override when set") {
    KeymapManager mgr;

    auto desc = makeAction("test.bind2", "Bind2", "Test",
                           KeyActionScope::global(), QKeySequence(Qt::Key_Space));
    mgr.registerAction(desc);
    mgr.setUserOverride("test.bind2", QKeySequence(Qt::Key_Return));

    CHECK(mgr.bindingFor("test.bind2") == QKeySequence(Qt::Key_Return));
}

TEST_CASE("clearUserOverride reverts to default") {
    KeymapManager mgr;

    auto desc = makeAction("test.clear", "Clear", "Test",
                           KeyActionScope::global(), QKeySequence(Qt::Key_A));
    mgr.registerAction(desc);
    mgr.setUserOverride("test.clear", QKeySequence(Qt::Key_B));
    mgr.clearUserOverride("test.clear");

    CHECK(mgr.bindingFor("test.clear") == QKeySequence(Qt::Key_A));
}

TEST_CASE("clearAllOverrides reverts all") {
    KeymapManager mgr;

    mgr.registerAction(makeAction("test.x", "X", "Test",
                                  KeyActionScope::global(), QKeySequence(Qt::Key_X)));
    mgr.registerAction(makeAction("test.y", "Y", "Test",
                                  KeyActionScope::global(), QKeySequence(Qt::Key_Y)));
    mgr.setUserOverride("test.x", QKeySequence(Qt::Key_1));
    mgr.setUserOverride("test.y", QKeySequence(Qt::Key_2));
    mgr.clearAllOverrides();

    CHECK(mgr.bindingFor("test.x") == QKeySequence(Qt::Key_X));
    CHECK(mgr.bindingFor("test.y") == QKeySequence(Qt::Key_Y));
}

// ============================================================
// Enable/disable
// ============================================================

TEST_CASE("setActionEnabled disables action") {
    KeymapManager mgr;

    mgr.registerAction(makeAction("test.en", "En", "Test",
                                  KeyActionScope::global(), QKeySequence(Qt::Key_E)));
    CHECK(mgr.isActionEnabled("test.en"));

    mgr.setActionEnabled("test.en", false);
    CHECK_FALSE(mgr.isActionEnabled("test.en"));

    mgr.setActionEnabled("test.en", true);
    CHECK(mgr.isActionEnabled("test.en"));
}

// ============================================================
// Conflict detection
// ============================================================

TEST_CASE("detectConflicts flags same key+scope") {
    KeymapManager mgr;

    mgr.registerAction(makeAction("test.c1", "C1", "Test",
                                  KeyActionScope::global(), QKeySequence(Qt::Key_F5)));
    mgr.registerAction(makeAction("test.c2", "C2", "Test",
                                  KeyActionScope::global(), QKeySequence(Qt::Key_F5)));

    auto conflicts = mgr.detectConflicts();
    REQUIRE(conflicts.size() == 1);
    CHECK(conflicts[0].action_id_a == "test.c1");
    CHECK(conflicts[0].action_id_b == "test.c2");
    CHECK(conflicts[0].key == QKeySequence(Qt::Key_F5));
}

TEST_CASE("detectConflicts allows same key in different scopes") {
    KeymapManager mgr;

    auto const media_type = EditorTypeId("MediaWidget");
    auto const scatter_type = EditorTypeId("ScatterPlotWidget");

    mgr.registerAction(makeAction("test.d1", "D1", "Media",
                                  KeyActionScope::editorFocused(media_type),
                                  QKeySequence(Qt::Key_1)));
    mgr.registerAction(makeAction("test.d2", "D2", "Scatter",
                                  KeyActionScope::editorFocused(scatter_type),
                                  QKeySequence(Qt::Key_1)));

    CHECK(mgr.detectConflicts().empty());
}

TEST_CASE("detectConflicts with override-created conflict") {
    KeymapManager mgr;

    mgr.registerAction(makeAction("test.oc1", "OC1", "Test",
                                  KeyActionScope::global(), QKeySequence(Qt::Key_A)));
    mgr.registerAction(makeAction("test.oc2", "OC2", "Test",
                                  KeyActionScope::global(), QKeySequence(Qt::Key_B)));

    // No conflict initially
    CHECK(mgr.detectConflicts().empty());

    // Override creates conflict
    mgr.setUserOverride("test.oc2", QKeySequence(Qt::Key_A));
    auto conflicts = mgr.detectConflicts();
    REQUIRE(conflicts.size() == 1);
}

// ============================================================
// Scope resolution
// ============================================================

TEST_CASE("resolveKeyPress returns EditorFocused over AlwaysRouted over Global") {
    KeymapManager mgr;

    auto const media_type = EditorTypeId("MediaWidget");
    auto const timeline_type = EditorTypeId("TimeScrollBar");

    // Same key bound in all three scopes
    mgr.registerAction(makeAction("global.action", "Global", "Global",
                                  KeyActionScope::global(), QKeySequence(Qt::Key_Space)));
    mgr.registerAction(makeAction("routed.action", "Routed", "Timeline",
                                  KeyActionScope::alwaysRouted(timeline_type),
                                  QKeySequence(Qt::Key_Space)));
    mgr.registerAction(makeAction("focused.action", "Focused", "Media",
                                  KeyActionScope::editorFocused(media_type),
                                  QKeySequence(Qt::Key_Space)));

    // With MediaWidget focused: EditorFocused wins
    auto result = mgr.resolveKeyPress(QKeySequence(Qt::Key_Space), media_type);
    REQUIRE(result.has_value());
    CHECK(*result == "focused.action");

    // With no focus: AlwaysRouted wins over Global
    auto result2 = mgr.resolveKeyPress(QKeySequence(Qt::Key_Space), EditorTypeId{});
    REQUIRE(result2.has_value());
    CHECK(*result2 == "routed.action");
}

TEST_CASE("resolveKeyPress returns Global when no focused or routed match") {
    KeymapManager mgr;

    mgr.registerAction(makeAction("g.only", "Global Only", "Global",
                                  KeyActionScope::global(), QKeySequence(Qt::Key_F12)));

    auto result = mgr.resolveKeyPress(QKeySequence(Qt::Key_F12), EditorTypeId{});
    REQUIRE(result.has_value());
    CHECK(*result == "g.only");
}

TEST_CASE("resolveKeyPress returns nullopt for unbound key") {
    KeymapManager mgr;

    mgr.registerAction(makeAction("test.nope", "Nope", "Test",
                                  KeyActionScope::global(), QKeySequence(Qt::Key_A)));

    auto result = mgr.resolveKeyPress(QKeySequence(Qt::Key_Z), EditorTypeId{});
    CHECK_FALSE(result.has_value());
}

TEST_CASE("resolveKeyPress respects disabled actions") {
    KeymapManager mgr;

    mgr.registerAction(makeAction("test.dis", "Disabled", "Test",
                                  KeyActionScope::global(), QKeySequence(Qt::Key_D)));
    mgr.setActionEnabled("test.dis", false);

    auto result = mgr.resolveKeyPress(QKeySequence(Qt::Key_D), EditorTypeId{});
    CHECK_FALSE(result.has_value());
}

TEST_CASE("resolveKeyPress uses override binding") {
    KeymapManager mgr;

    mgr.registerAction(makeAction("test.overridden", "ORide", "Test",
                                  KeyActionScope::global(), QKeySequence(Qt::Key_A)));
    mgr.setUserOverride("test.overridden", QKeySequence(Qt::Key_B));

    // Old key no longer matches
    CHECK_FALSE(mgr.resolveKeyPress(QKeySequence(Qt::Key_A), EditorTypeId{}).has_value());

    // New key matches
    auto result = mgr.resolveKeyPress(QKeySequence(Qt::Key_B), EditorTypeId{});
    REQUIRE(result.has_value());
    CHECK(*result == "test.overridden");
}

// ============================================================
// Adapter management and dispatch
// ============================================================

TEST_CASE("registerAdapter and dispatchAction route to handler") {
    KeymapManager mgr;

    auto const media_type = EditorTypeId("MediaWidget");
    auto const instance = EditorInstanceId::generate();

    // Create adapter (QObject parenting: mgr owns it for this test)
    auto * adapter = new KeyActionAdapter(&mgr);
    adapter->setTypeId(media_type);
    adapter->setInstanceId(instance);

    bool handled = false;
    adapter->setHandler([&handled](QString const & action_id) -> bool {
        if (action_id == "media.assign_group_1") {
            handled = true;
            return true;
        }
        return false;
    });

    mgr.registerAdapter(adapter);

    // Dispatch should reach the adapter
    REQUIRE(mgr.dispatchAction("media.assign_group_1", media_type));
    CHECK(handled);
}

TEST_CASE("dispatchAction returns false when no adapter matches") {
    KeymapManager mgr;

    CHECK_FALSE(mgr.dispatchAction(
            "test.nothing", EditorTypeId("Unknown")));
}

TEST_CASE("adapter auto-deregisters on destruction") {
    KeymapManager mgr;

    auto const test_type = EditorTypeId("TestWidget");
    {
        auto * adapter = new KeyActionAdapter(nullptr);
        adapter->setTypeId(test_type);
        adapter->setHandler([](QString const &) { return true; });
        mgr.registerAdapter(adapter);

        // Dispatch works while adapter is alive
        REQUIRE(mgr.dispatchAction("some.action", test_type));

        delete adapter;
    }

    // Dispatch fails after adapter is destroyed
    CHECK_FALSE(mgr.dispatchAction("some.action", test_type));
}

TEST_CASE("dispatchAction targets specific instance when requested") {
    KeymapManager mgr;

    auto const type = EditorTypeId("MultiWidget");
    auto const id_a = EditorInstanceId::generate();
    auto const id_b = EditorInstanceId::generate();

    int call_count_a = 0;
    int call_count_b = 0;

    auto * adapter_a = new KeyActionAdapter(&mgr);
    adapter_a->setTypeId(type);
    adapter_a->setInstanceId(id_a);
    adapter_a->setHandler([&call_count_a](QString const &) {
        ++call_count_a;
        return true;
    });

    auto * adapter_b = new KeyActionAdapter(&mgr);
    adapter_b->setTypeId(type);
    adapter_b->setInstanceId(id_b);
    adapter_b->setHandler([&call_count_b](QString const &) {
        ++call_count_b;
        return true;
    });

    mgr.registerAdapter(adapter_a);
    mgr.registerAdapter(adapter_b);

    // Dispatch to specific instance
    mgr.dispatchAction("test.act", type, id_b);
    CHECK(call_count_a == 0);
    CHECK(call_count_b == 1);
}

// ============================================================
// Override serialization round-trip
// ============================================================

TEST_CASE("exportOverrides and importOverrides round-trip") {
    KeymapManager mgr;

    mgr.registerAction(makeAction("test.rt1", "RT1", "Test",
                                  KeyActionScope::global(), QKeySequence(Qt::Key_A)));
    mgr.registerAction(makeAction("test.rt2", "RT2", "Test",
                                  KeyActionScope::global(), QKeySequence(Qt::Key_B)));

    mgr.setUserOverride("test.rt1", QKeySequence(Qt::Key_Z));
    mgr.setActionEnabled("test.rt2", false);

    auto exported = mgr.exportOverrides();
    REQUIRE(exported.size() == 2);

    // Create a fresh manager and import
    KeymapManager mgr2;
    mgr2.registerAction(makeAction("test.rt1", "RT1", "Test",
                                   KeyActionScope::global(), QKeySequence(Qt::Key_A)));
    mgr2.registerAction(makeAction("test.rt2", "RT2", "Test",
                                   KeyActionScope::global(), QKeySequence(Qt::Key_B)));

    mgr2.importOverrides(exported);

    CHECK(mgr2.bindingFor("test.rt1") == QKeySequence(Qt::Key_Z));
    CHECK_FALSE(mgr2.isActionEnabled("test.rt2"));
}
