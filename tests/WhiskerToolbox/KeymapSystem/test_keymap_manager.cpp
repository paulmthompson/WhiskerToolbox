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

#include <array>

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
            static QCoreApplication const app(argc, const_cast<char **>(argv));
        }
    }
};

[[maybe_unused]] QtAppGuard const qt_guard;

KeyActionDescriptor makeAction(
        QString id,
        QString name,
        QString category,
        KeyActionScope scope,
        QKeySequence const & key = {}) {
    return {std::move(id), std::move(name), std::move(category), std::move(scope), key};
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
    KeymapManager const mgr;

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

// ============================================================
// Timeline action registration and dispatch (Step 2.1)
// ============================================================

TEST_CASE("timeline actions register with correct scope and defaults") {
    KeymapManager mgr;

    auto const scope = KeyActionScope::alwaysRouted(
            EditorTypeId(QStringLiteral("TimeScrollBar")));
    QString const category = QStringLiteral("Timeline");

    mgr.registerAction({.action_id = QStringLiteral("timeline.play_pause"),
                        .display_name = QStringLiteral("Play / Pause"),
                        .category = category,
                        .scope = scope,
                        .default_binding = QKeySequence(Qt::Key_Space)});
    mgr.registerAction({.action_id = QStringLiteral("timeline.next_frame"),
                        .display_name = QStringLiteral("Next Frame"),
                        .category = category,
                        .scope = scope,
                        .default_binding = QKeySequence(Qt::Key_Right)});
    mgr.registerAction({.action_id = QStringLiteral("timeline.prev_frame"),
                        .display_name = QStringLiteral("Previous Frame"),
                        .category = category,
                        .scope = scope,
                        .default_binding = QKeySequence(Qt::Key_Left)});
    mgr.registerAction({.action_id = QStringLiteral("timeline.jump_forward"),
                        .display_name = QStringLiteral("Jump Forward"),
                        .category = category,
                        .scope = scope,
                        .default_binding = QKeySequence(Qt::CTRL | Qt::Key_Right)});
    mgr.registerAction({.action_id = QStringLiteral("timeline.jump_backward"),
                        .display_name = QStringLiteral("Jump Backward"),
                        .category = category,
                        .scope = scope,
                        .default_binding = QKeySequence(Qt::CTRL | Qt::Key_Left)});

    CHECK(mgr.allActions().size() == 5);

    // Verify defaults
    CHECK(mgr.bindingFor("timeline.play_pause") == QKeySequence(Qt::Key_Space));
    CHECK(mgr.bindingFor("timeline.next_frame") == QKeySequence(Qt::Key_Right));
    CHECK(mgr.bindingFor("timeline.prev_frame") == QKeySequence(Qt::Key_Left));
    CHECK(mgr.bindingFor("timeline.jump_forward") == QKeySequence(Qt::CTRL | Qt::Key_Right));
    CHECK(mgr.bindingFor("timeline.jump_backward") == QKeySequence(Qt::CTRL | Qt::Key_Left));

    // Verify scope
    auto desc = mgr.action("timeline.play_pause");
    REQUIRE(desc.has_value());
    CHECK(desc->scope.kind == KeyActionScopeKind::AlwaysRouted);
    CHECK(desc->scope.editor_type_id == EditorTypeId("TimeScrollBar"));
    CHECK(desc->category == "Timeline");
}

TEST_CASE("timeline actions resolve via AlwaysRouted regardless of focus") {
    KeymapManager mgr;

    auto const scope = KeyActionScope::alwaysRouted(
            EditorTypeId(QStringLiteral("TimeScrollBar")));

    mgr.registerAction({.action_id = QStringLiteral("timeline.play_pause"),
                        .display_name = QStringLiteral("Play / Pause"),
                        .category = QStringLiteral("Timeline"),
                        .scope = scope,
                        .default_binding = QKeySequence(Qt::Key_Space)});

    // Resolves with no focus
    auto result1 = mgr.resolveKeyPress(QKeySequence(Qt::Key_Space), EditorTypeId{});
    REQUIRE(result1.has_value());
    CHECK(*result1 == "timeline.play_pause");

    // Resolves even when a different editor is focused
    auto result2 = mgr.resolveKeyPress(
            QKeySequence(Qt::Key_Space),
            EditorTypeId("SomeOtherWidget"));
    REQUIRE(result2.has_value());
    CHECK(*result2 == "timeline.play_pause");
}

TEST_CASE("timeline adapter dispatches actions to handler") {
    KeymapManager mgr;

    auto const tsb_type = EditorTypeId("TimeScrollBar");

    auto * adapter = new KeyActionAdapter(&mgr);
    adapter->setTypeId(tsb_type);

    QString last_action;
    adapter->setHandler([&last_action](QString const & action_id) -> bool {
        last_action = action_id;
        return true;
    });
    mgr.registerAdapter(adapter);

    // Dispatch each timeline action
    REQUIRE(mgr.dispatchAction("timeline.play_pause", tsb_type));
    CHECK(last_action == "timeline.play_pause");

    REQUIRE(mgr.dispatchAction("timeline.next_frame", tsb_type));
    CHECK(last_action == "timeline.next_frame");

    REQUIRE(mgr.dispatchAction("timeline.prev_frame", tsb_type));
    CHECK(last_action == "timeline.prev_frame");

    REQUIRE(mgr.dispatchAction("timeline.jump_forward", tsb_type));
    CHECK(last_action == "timeline.jump_forward");

    REQUIRE(mgr.dispatchAction("timeline.jump_backward", tsb_type));
    CHECK(last_action == "timeline.jump_backward");
}

TEST_CASE("timeline actions can be rebound via user override") {
    KeymapManager mgr;

    auto const scope = KeyActionScope::alwaysRouted(
            EditorTypeId(QStringLiteral("TimeScrollBar")));

    mgr.registerAction({.action_id = QStringLiteral("timeline.play_pause"),
                        .display_name = QStringLiteral("Play / Pause"),
                        .category = QStringLiteral("Timeline"),
                        .scope = scope,
                        .default_binding = QKeySequence(Qt::Key_Space)});

    // Override Space → Enter
    mgr.setUserOverride("timeline.play_pause", QKeySequence(Qt::Key_Return));

    // Space no longer resolves
    CHECK_FALSE(mgr.resolveKeyPress(QKeySequence(Qt::Key_Space), EditorTypeId{}).has_value());

    // Enter resolves instead
    auto result = mgr.resolveKeyPress(QKeySequence(Qt::Key_Return), EditorTypeId{});
    REQUIRE(result.has_value());
    CHECK(*result == "timeline.play_pause");
}

// ============================================================
// Media group-assignment action registration and dispatch (Step 2.2)
// ============================================================

TEST_CASE("media group actions register with EditorFocused scope and digit defaults") {
    KeymapManager mgr;

    auto const scope = KeyActionScope::editorFocused(
            EditorTypeId(QStringLiteral("MediaWidget")));
    QString const category = QStringLiteral("Media Viewer");

    constexpr std::array<Qt::Key, 9> keys = {
            Qt::Key_1, Qt::Key_2, Qt::Key_3,
            Qt::Key_4, Qt::Key_5, Qt::Key_6,
            Qt::Key_7, Qt::Key_8, Qt::Key_9};

    for (int i = 0; i < 9; ++i) {
        mgr.registerAction({.action_id = QStringLiteral("media.assign_group_%1").arg(i + 1),
                            .display_name = QStringLiteral("Assign to Group %1").arg(i + 1),
                            .category = category,
                            .scope = scope,
                            .default_binding = QKeySequence(keys[static_cast<size_t>(i)])});
    }

    CHECK(mgr.allActions().size() == 9);

    // Verify default bindings
    for (int i = 0; i < 9; ++i) {
        auto id = QStringLiteral("media.assign_group_%1").arg(i + 1);
        CHECK(mgr.bindingFor(id) == QKeySequence(keys[static_cast<size_t>(i)]));
    }

    // Verify scope and category
    auto desc = mgr.action("media.assign_group_1");
    REQUIRE(desc.has_value());
    CHECK(desc->scope.kind == KeyActionScopeKind::EditorFocused);
    CHECK(desc->scope.editor_type_id == EditorTypeId("MediaWidget"));
    CHECK(desc->category == "Media Viewer");
}

TEST_CASE("media group actions resolve only when MediaWidget is focused") {
    KeymapManager mgr;

    auto const scope = KeyActionScope::editorFocused(
            EditorTypeId(QStringLiteral("MediaWidget")));

    mgr.registerAction({.action_id = QStringLiteral("media.assign_group_1"),
                        .display_name = QStringLiteral("Assign to Group 1"),
                        .category = QStringLiteral("Media Viewer"),
                        .scope = scope,
                        .default_binding = QKeySequence(Qt::Key_1)});

    // Resolves when MediaWidget is focused
    auto result = mgr.resolveKeyPress(
            QKeySequence(Qt::Key_1),
            EditorTypeId("MediaWidget"));
    REQUIRE(result.has_value());
    CHECK(*result == "media.assign_group_1");

    // Does NOT resolve when another widget is focused
    CHECK_FALSE(mgr.resolveKeyPress(
                           QKeySequence(Qt::Key_1),
                           EditorTypeId("TimeScrollBar"))
                        .has_value());

    // Does NOT resolve with no focus
    CHECK_FALSE(mgr.resolveKeyPress(
                           QKeySequence(Qt::Key_1),
                           EditorTypeId{})
                        .has_value());
}

TEST_CASE("media group adapter dispatches actions to handler") {
    KeymapManager mgr;

    auto const media_type = EditorTypeId("MediaWidget");

    auto * adapter = new KeyActionAdapter(&mgr);
    adapter->setTypeId(media_type);

    QString last_action;
    adapter->setHandler([&last_action](QString const & action_id) -> bool {
        last_action = action_id;
        return true;
    });
    mgr.registerAdapter(adapter);

    // Dispatch each group action
    for (int i = 1; i <= 9; ++i) {
        auto id = QStringLiteral("media.assign_group_%1").arg(i);
        REQUIRE(mgr.dispatchAction(id, media_type));
        CHECK(last_action == id);
    }
}

TEST_CASE("media group actions can be rebound via user override") {
    KeymapManager mgr;

    auto const scope = KeyActionScope::editorFocused(
            EditorTypeId(QStringLiteral("MediaWidget")));

    mgr.registerAction({.action_id = QStringLiteral("media.assign_group_1"),
                        .display_name = QStringLiteral("Assign to Group 1"),
                        .category = QStringLiteral("Media Viewer"),
                        .scope = scope,
                        .default_binding = QKeySequence(Qt::Key_1)});

    // Override Key_1 → Key_F1
    mgr.setUserOverride("media.assign_group_1", QKeySequence(Qt::Key_F1));

    auto const media_type = EditorTypeId("MediaWidget");

    // Key_1 no longer resolves
    CHECK_FALSE(mgr.resolveKeyPress(QKeySequence(Qt::Key_1), media_type).has_value());

    // Key_F1 resolves instead
    auto result = mgr.resolveKeyPress(QKeySequence(Qt::Key_F1), media_type);
    REQUIRE(result.has_value());
    CHECK(*result == "media.assign_group_1");
}

// ============================================================
// Active target management
// ============================================================

TEST_CASE("activeTarget returns invalid ID when no target is set") {
    KeymapManager mgr;
    auto const type = EditorTypeId("TestWidget");
    CHECK_FALSE(mgr.activeTarget(type).isValid());
}

TEST_CASE("setActiveTarget and activeTarget round-trip") {
    KeymapManager mgr;
    auto const type = EditorTypeId("TestWidget");
    auto const id = EditorInstanceId::generate();

    mgr.setActiveTarget(type, id);
    CHECK(mgr.activeTarget(type) == id);
}

TEST_CASE("setActiveTarget emits activeTargetChanged") {
    KeymapManager mgr;
    auto const type = EditorTypeId("TestWidget");
    auto const id = EditorInstanceId::generate();

    EditorTypeId received_type;
    EditorInstanceId received_id;
    QObject::connect(&mgr, &KeymapManager::activeTargetChanged,
                     [&](EditorTypeId t, EditorInstanceId i) {
                         received_type = t;
                         received_id = i;
                     });

    mgr.setActiveTarget(type, id);
    CHECK(received_type == type);
    CHECK(received_id == id);
}

TEST_CASE("setActiveTarget with empty ID clears the target") {
    KeymapManager mgr;
    auto const type = EditorTypeId("TestWidget");
    auto const id = EditorInstanceId::generate();

    mgr.setActiveTarget(type, id);
    REQUIRE(mgr.activeTarget(type).isValid());

    mgr.setActiveTarget(type, {});
    CHECK_FALSE(mgr.activeTarget(type).isValid());
}

TEST_CASE("cycleActiveTarget cycles through adapters of the same type") {
    KeymapManager mgr;
    auto const type = EditorTypeId("MultiWidget");
    auto const id_a = EditorInstanceId::generate();
    auto const id_b = EditorInstanceId::generate();
    auto const id_c = EditorInstanceId::generate();

    auto * adapter_a = new KeyActionAdapter(&mgr);
    adapter_a->setTypeId(type);
    adapter_a->setInstanceId(id_a);
    adapter_a->setHandler([](QString const &) { return true; });

    auto * adapter_b = new KeyActionAdapter(&mgr);
    adapter_b->setTypeId(type);
    adapter_b->setInstanceId(id_b);
    adapter_b->setHandler([](QString const &) { return true; });

    auto * adapter_c = new KeyActionAdapter(&mgr);
    adapter_c->setTypeId(type);
    adapter_c->setInstanceId(id_c);
    adapter_c->setHandler([](QString const &) { return true; });

    mgr.registerAdapter(adapter_a);
    mgr.registerAdapter(adapter_b);
    mgr.registerAdapter(adapter_c);

    // First cycle: no active target → selects first
    mgr.cycleActiveTarget(type);
    CHECK(mgr.activeTarget(type) == id_a);

    // Second cycle: a → b
    mgr.cycleActiveTarget(type);
    CHECK(mgr.activeTarget(type) == id_b);

    // Third cycle: b → c
    mgr.cycleActiveTarget(type);
    CHECK(mgr.activeTarget(type) == id_c);

    // Fourth cycle: c → wraps to a
    mgr.cycleActiveTarget(type);
    CHECK(mgr.activeTarget(type) == id_a);
}

TEST_CASE("cycleActiveTarget with single adapter stays on it") {
    KeymapManager mgr;
    auto const type = EditorTypeId("SingleWidget");
    auto const id = EditorInstanceId::generate();

    auto * adapter = new KeyActionAdapter(&mgr);
    adapter->setTypeId(type);
    adapter->setInstanceId(id);
    adapter->setHandler([](QString const &) { return true; });
    mgr.registerAdapter(adapter);

    mgr.cycleActiveTarget(type);
    CHECK(mgr.activeTarget(type) == id);

    // Cycling again stays on the same adapter
    mgr.cycleActiveTarget(type);
    CHECK(mgr.activeTarget(type) == id);
}

TEST_CASE("cycleActiveTarget with no adapters does nothing") {
    KeymapManager mgr;
    auto const type = EditorTypeId("EmptyWidget");

    mgr.cycleActiveTarget(type);
    CHECK_FALSE(mgr.activeTarget(type).isValid());
}

TEST_CASE("destroying active target adapter clears it") {
    KeymapManager mgr;
    auto const type = EditorTypeId("DestroyWidget");
    auto const id_a = EditorInstanceId::generate();
    auto const id_b = EditorInstanceId::generate();

    auto * adapter_a = new KeyActionAdapter(&mgr);
    adapter_a->setTypeId(type);
    adapter_a->setInstanceId(id_a);
    adapter_a->setHandler([](QString const &) { return true; });

    auto * adapter_b = new KeyActionAdapter(&mgr);
    adapter_b->setTypeId(type);
    adapter_b->setInstanceId(id_b);
    adapter_b->setHandler([](QString const &) { return true; });

    mgr.registerAdapter(adapter_a);
    mgr.registerAdapter(adapter_b);

    // Set active target to adapter_a
    mgr.setActiveTarget(type, id_a);
    REQUIRE(mgr.activeTarget(type) == id_a);

    // Destroy adapter_a → active target should be cleared
    delete adapter_a;
    CHECK_FALSE(mgr.activeTarget(type).isValid());
}

TEST_CASE("AlwaysRouted dispatch uses active target when set") {
    KeymapManager mgr;
    auto const type = EditorTypeId("IntervalInspector");
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

    // Without active target: dispatches to first matching adapter
    mgr.dispatchAction("test.action", type);
    CHECK(call_count_a == 1);
    CHECK(call_count_b == 0);

    // Set active target to adapter_b
    mgr.setActiveTarget(type, id_b);

    // Now dispatch — should go to adapter_b
    mgr.dispatchAction("test.action", type, mgr.activeTarget(type));
    CHECK(call_count_a == 1);// Unchanged
    CHECK(call_count_b == 1);// Got the action
}

TEST_CASE("adapterInstancesForType returns all instances of a type") {
    KeymapManager mgr;
    auto const type_a = EditorTypeId("TypeA");
    auto const type_b = EditorTypeId("TypeB");
    auto const id_1 = EditorInstanceId::generate();
    auto const id_2 = EditorInstanceId::generate();
    auto const id_3 = EditorInstanceId::generate();

    auto * a1 = new KeyActionAdapter(&mgr);
    a1->setTypeId(type_a);
    a1->setInstanceId(id_1);
    mgr.registerAdapter(a1);

    auto * a2 = new KeyActionAdapter(&mgr);
    a2->setTypeId(type_a);
    a2->setInstanceId(id_2);
    mgr.registerAdapter(a2);

    auto * b1 = new KeyActionAdapter(&mgr);
    b1->setTypeId(type_b);
    b1->setInstanceId(id_3);
    mgr.registerAdapter(b1);

    auto const instances = mgr.adapterInstancesForType(type_a);
    REQUIRE(instances.size() == 2);
    CHECK(instances[0] == id_1);
    CHECK(instances[1] == id_2);

    auto const b_instances = mgr.adapterInstancesForType(type_b);
    REQUIRE(b_instances.size() == 1);
    CHECK(b_instances[0] == id_3);
}

// ============================================================
// End-to-end integration: DigitalIntervalSeries multi-instance scenario
// ============================================================

TEST_CASE("E2E: AlwaysRouted actions reach the correct inspector instance") {
    KeymapManager mgr;

    // --- Step 1: Register actions (mirrors DataInspectorWidgetRegistration.cpp) ---
    auto const inspector_type = EditorTypeId("DigitalIntervalSeriesInspector");
    auto const scope = KeyActionScope::alwaysRouted(inspector_type);
    QString const category = "Digital Interval Series";

    mgr.registerAction({.action_id = QStringLiteral("digital_interval_series.mark_contact_start"),
                        .display_name = QStringLiteral("Mark Contact Start"),
                        .category = category,
                        .scope = scope,
                        .default_binding = QKeySequence()});

    mgr.registerAction({.action_id = QStringLiteral("digital_interval_series.cycle_active_target"),
                        .display_name = QStringLiteral("Cycle Active Interval Inspector"),
                        .category = category,
                        .scope = KeyActionScope::global(),
                        .default_binding = QKeySequence()});

    // Bind keys (user would do this in the keybinding editor)
    mgr.setUserOverride("digital_interval_series.mark_contact_start", QKeySequence(Qt::Key_Q));
    mgr.setUserOverride("digital_interval_series.cycle_active_target", QKeySequence(Qt::Key_Tab));

    // --- Step 2: Register the global cycle adapter (mirrors registration code) ---
    auto * cycle_adapter = new KeyActionAdapter(&mgr);
    cycle_adapter->setTypeId(EditorTypeId{});// Global scope = empty type
    cycle_adapter->setHandler(
            [&mgr, inspector_type](QString const & action_id) -> bool {
                if (action_id == "digital_interval_series.cycle_active_target") {
                    mgr.cycleActiveTarget(inspector_type);
                    return true;
                }
                return false;
            });
    mgr.registerAdapter(cycle_adapter);

    // --- Step 3: Create two inspector adapters (mirrors setKeymapManager) ---
    auto const id_inspector1 = EditorInstanceId::generate();
    auto const id_inspector2 = EditorInstanceId::generate();

    int handler_calls_1 = 0;
    int handler_calls_2 = 0;

    auto * adapter1 = new KeyActionAdapter(&mgr);
    adapter1->setTypeId(inspector_type);
    adapter1->setInstanceId(id_inspector1);
    adapter1->setHandler([&handler_calls_1](QString const & action_id) -> bool {
        if (action_id == "digital_interval_series.mark_contact_start") {
            ++handler_calls_1;
            return true;
        }
        return false;
    });
    mgr.registerAdapter(adapter1);

    auto * adapter2 = new KeyActionAdapter(&mgr);
    adapter2->setTypeId(inspector_type);
    adapter2->setInstanceId(id_inspector2);
    adapter2->setHandler([&handler_calls_2](QString const & action_id) -> bool {
        if (action_id == "digital_interval_series.mark_contact_start") {
            ++handler_calls_2;
            return true;
        }
        return false;
    });
    mgr.registerAdapter(adapter2);

    // --- Step 4: Simulate keypress Q (mark_contact_start) with NO active target ---
    // This is what eventFilter does: resolve → determine target → dispatch
    {
        auto const resolved = mgr.resolveKeyPress(QKeySequence(Qt::Key_Q), EditorTypeId{});
        REQUIRE(resolved.has_value());
        CHECK(*resolved == "digital_interval_series.mark_contact_start");

        auto const desc = mgr.action(*resolved);
        REQUIRE(desc.has_value());
        CHECK(desc->scope.kind == KeyActionScopeKind::AlwaysRouted);

        EditorTypeId target_type = desc->scope.editor_type_id;
        EditorInstanceId target_instance = mgr.activeTarget(target_type);

        // No active target set yet — target_instance should be invalid
        CHECK_FALSE(target_instance.isValid());

        // Dispatch should go to the FIRST adapter (adapter1)
        bool handled = mgr.dispatchAction(*resolved, target_type, target_instance);
        REQUIRE(handled);
        CHECK(handler_calls_1 == 1);
        CHECK(handler_calls_2 == 0);
    }

    // --- Step 5: Simulate keypress Tab (cycle_active_target) ---
    {
        auto const resolved = mgr.resolveKeyPress(QKeySequence(Qt::Key_Tab), EditorTypeId{});
        REQUIRE(resolved.has_value());
        CHECK(*resolved == "digital_interval_series.cycle_active_target");

        auto const desc = mgr.action(*resolved);
        REQUIRE(desc.has_value());
        CHECK(desc->scope.kind == KeyActionScopeKind::Global);

        EditorTypeId target_type{};// Global = empty
        bool handled = mgr.dispatchAction(*resolved, target_type);
        REQUIRE(handled);

        // After cycling once, active target should be inspector1
        CHECK(mgr.activeTarget(inspector_type) == id_inspector1);
    }

    // --- Step 6: Cycle again → should switch to inspector2 ---
    {
        auto const resolved = mgr.resolveKeyPress(QKeySequence(Qt::Key_Tab), EditorTypeId{});
        auto const desc = mgr.action(*resolved);
        bool handled = mgr.dispatchAction(*resolved, EditorTypeId{});
        REQUIRE(handled);
        CHECK(mgr.activeTarget(inspector_type) == id_inspector2);
    }

    // --- Step 7: Now press Q again — should go to inspector2 (active target) ---
    {
        auto const resolved = mgr.resolveKeyPress(QKeySequence(Qt::Key_Q), EditorTypeId{});
        REQUIRE(resolved.has_value());

        auto const desc = mgr.action(*resolved);
        EditorTypeId target_type = desc->scope.editor_type_id;
        EditorInstanceId target_instance = mgr.activeTarget(target_type);

        CHECK(target_instance == id_inspector2);

        bool handled = mgr.dispatchAction(*resolved, target_type, target_instance);
        REQUIRE(handled);
        CHECK(handler_calls_1 == 1);// Unchanged
        CHECK(handler_calls_2 == 1);// Got the action
    }

    // --- Step 8: Cycle again → wraps back to inspector1 ---
    {
        mgr.dispatchAction("digital_interval_series.cycle_active_target", EditorTypeId{});
        CHECK(mgr.activeTarget(inspector_type) == id_inspector1);
    }

    // --- Step 9: Press Q → should go to inspector1 now ---
    {
        auto const resolved = mgr.resolveKeyPress(QKeySequence(Qt::Key_Q), EditorTypeId{});
        auto const desc = mgr.action(*resolved);
        EditorTypeId target_type = desc->scope.editor_type_id;
        EditorInstanceId target_instance = mgr.activeTarget(target_type);

        bool handled = mgr.dispatchAction(*resolved, target_type, target_instance);
        REQUIRE(handled);
        CHECK(handler_calls_1 == 2);// Got the action
        CHECK(handler_calls_2 == 1);// Unchanged
    }
}
