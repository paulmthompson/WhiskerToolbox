/**
 * @file StartupTrace.cpp
 * @brief Implementation of startup-phase tracing helpers
 */

#include "StartupTrace.hpp"

#include <QElapsedTimer>
#include <QMainWindow>
#include <QWindow>

#include <spdlog/spdlog.h>

namespace StateManagement {

namespace {

QElapsedTimer & startupTimer() {
    static QElapsedTimer timer = []() {
        QElapsedTimer t;
        t.start();
        return t;
    }();
    return timer;
}

bool g_recovery_dialog_open = false;

}// namespace

qint64 startupElapsedMs() {
    return startupTimer().elapsed();
}

void logStartupPhase(char const * phase) {
    spdlog::debug("[Startup] phase='{}' elapsed_ms={} recovery_dialog_open={}",
                  phase,
                  startupElapsedMs(),
                  g_recovery_dialog_open);
}

void logWindowState(char const * phase, QMainWindow const * window) {
    if (window == nullptr) {
        spdlog::debug("[Startup] window_state phase='{}' elapsed_ms={} window=null",
                      phase,
                      startupElapsedMs());
        return;
    }

    auto const geom = window->geometry();
    bool const exposed = window->windowHandle() != nullptr && window->windowHandle()->isExposed();

    spdlog::debug(
            "[Startup] window_state phase='{}' elapsed_ms={} recovery_dialog_open={} "
            "visible={} hidden={} exposed={} maximized={} geometry={}x{}@({},{}) window_state=0x{:x}",
            phase,
            startupElapsedMs(),
            g_recovery_dialog_open,
            window->isVisible(),
            window->isHidden(),
            exposed,
            window->isMaximized(),
            geom.width(),
            geom.height(),
            geom.x(),
            geom.y(),
            static_cast<unsigned>(window->windowState()));
}

void logSplitterMetrics(char const * phase,
                        int h_splitter_count,
                        int h_splitter_width,
                        int v_splitter_count,
                        int v_splitter_height,
                        int computed_left_width,
                        int computed_center_width,
                        int computed_right_width,
                        int computed_main_height,
                        int computed_bottom_height) {
    spdlog::debug(
            "[Startup] splitter_state phase='{}' elapsed_ms={} recovery_dialog_open={} "
            "h_count={} h_width={} v_count={} v_height={} "
            "sizes=({},{},{}) vertical=({},{})",
            phase,
            startupElapsedMs(),
            g_recovery_dialog_open,
            h_splitter_count,
            h_splitter_width,
            v_splitter_count,
            v_splitter_height,
            computed_left_width,
            computed_center_width,
            computed_right_width,
            computed_main_height,
            computed_bottom_height);

    if (h_splitter_width == 0 || v_splitter_height == 0) {
        spdlog::warn(
                "[Startup] splitter_state phase='{}' elapsed_ms={} recovery_dialog_open={} "
                "zero splitter dimension detected (h_width={} v_height={})",
                phase,
                startupElapsedMs(),
                g_recovery_dialog_open,
                h_splitter_width,
                v_splitter_height);
    }
}

void logReapplySplitterScheduled(int delay_ms) {
    spdlog::debug("[Startup] reapplySplitterSizes delay_ms={} elapsed_ms={} recovery_dialog_open={}",
                  delay_ms,
                  startupElapsedMs(),
                  g_recovery_dialog_open);
    logStartupPhase("reapplySplitterSizes scheduled");
}

void setRecoveryDialogOpen(bool open) {
    g_recovery_dialog_open = open;
    logStartupPhase(open ? "recovery_dialog_open" : "recovery_dialog_closed");
}

bool isRecoveryDialogOpen() {
    return g_recovery_dialog_open;
}

}// namespace StateManagement
