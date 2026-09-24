/**
 * @file Media_KeymapActions.cpp
 * @brief Keymap dispatch helpers for Media Viewer keyboard actions
 */

#include "Core/Media_KeymapActions.hpp"

#include "Core/MediaWidgetState.hpp"
#include "UI/Tools/MediaToolId.hpp"

#include "GroupManagementWidget/GroupManager.hpp"

#include <QString>

#include <cassert>

namespace {

[[nodiscard]] bool handleToolAction(QString const & action_id, MediaKeymapContext const & ctx) {
    assert(ctx.state != nullptr && "handleToolAction: state must not be null");

    MediaToolId tool = MediaToolId::None;
    if (action_id == QStringLiteral("media.tool.select")) {
        tool = MediaToolId::Select;
    } else if (action_id == QStringLiteral("media.tool.pen")) {
        tool = MediaToolId::Pen;
    } else if (action_id == QStringLiteral("media.tool.eraser")) {
        tool = MediaToolId::Eraser;
    } else if (action_id == QStringLiteral("media.tool.smooth")) {
        tool = MediaToolId::Smooth;
    } else if (action_id == QStringLiteral("media.tool.none")) {
        tool = MediaToolId::None;
    } else {
        return false;
    }

    ctx.state->setActiveMediaTool(tool);
    return true;
}

[[nodiscard]] bool handlePenCycleAppendEndpoint(MediaKeymapContext const & ctx) {
    assert(ctx.state != nullptr && "handlePenCycleAppendEndpoint: state must not be null");

    if (ctx.state->activeMediaTool() != MediaToolId::Pen) {
        return false;
    }

    auto prefs = ctx.state->linePrefs();
    prefs.append_endpoint = cycleLineAppendEndpoint(prefs.append_endpoint);
    ctx.state->setLinePrefs(prefs);
    return true;
}

[[nodiscard]] bool handleGroupAssignAction(QString const & action_id, MediaKeymapContext const & ctx) {
    if (!action_id.startsWith(QStringLiteral("media.assign_group_"))) {
        return false;
    }

    if (!ctx.group_manager || !ctx.selected_entities || ctx.selected_entities->empty()) {
        return false;
    }

    bool ok = false;
    int const group_number = action_id.mid(QStringLiteral("media.assign_group_").length()).toInt(&ok);
    if (!ok || group_number < 1 || group_number > 9) {
        return false;
    }

    auto groups = ctx.group_manager->getGroupsForContextMenu();
    if (group_number > static_cast<int>(groups.size())) {
        return false;
    }

    auto it = groups.begin();
    std::advance(it, group_number - 1);
    int const group_id = it->first;

    ctx.group_manager->assignEntitiesToGroup(group_id, *ctx.selected_entities);

    if (ctx.clear_selection) {
        ctx.clear_selection();
    }
    if (ctx.refresh_canvas) {
        ctx.refresh_canvas();
    }

    return true;
}

}// namespace

LineAppendEndpoint cycleLineAppendEndpoint(LineAppendEndpoint endpoint) {
    switch (endpoint) {
        case LineAppendEndpoint::Tip:
            return LineAppendEndpoint::Base;
        case LineAppendEndpoint::Base:
            return LineAppendEndpoint::Nearest;
        case LineAppendEndpoint::Nearest:
            return LineAppendEndpoint::Tip;
    }
    return LineAppendEndpoint::Tip;
}

bool handleMediaKeyAction(QString const & action_id, MediaKeymapContext const & ctx) {
    if (handleToolAction(action_id, ctx)) {
        return true;
    }

    if (action_id == QStringLiteral("media.pen.cycle_append_endpoint")) {
        return handlePenCycleAppendEndpoint(ctx);
    }

    return handleGroupAssignAction(action_id, ctx);
}
