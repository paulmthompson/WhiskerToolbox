#ifndef STARTUP_TRACE_HPP
#define STARTUP_TRACE_HPP

/**
 * @file StartupTrace.hpp
 * @brief Monotonic startup-phase tracing for crash-recovery diagnostics
 *
 * Emits spdlog messages tagged `[Startup]` at debug level. Enable with
 * `WhiskerToolbox --debug`.
 */

#include <QtGlobal>

class QMainWindow;

namespace StateManagement {

/// @return Milliseconds since the first startup trace call (monotonic).
[[nodiscard]] qint64 startupElapsedMs();

/// Log a named startup phase with elapsed time.
void logStartupPhase(char const * phase);

/// Log main-window visibility and geometry for a startup phase.
void logWindowState(char const * phase, QMainWindow const * window);

/// Log horizontal/vertical splitter metrics for layout-timing diagnosis.
void logSplitterMetrics(char const * phase,
                        int h_splitter_count,
                        int h_splitter_width,
                        int v_splitter_count,
                        int v_splitter_height,
                        int computed_left_width,
                        int computed_center_width,
                        int computed_right_width,
                        int computed_main_height,
                        int computed_bottom_height);

/// Log that a deferred splitter resize was scheduled.
void logReapplySplitterScheduled(int delay_ms);

/// Mark whether the crash-recovery modal dialog is currently open.
void setRecoveryDialogOpen(bool open);

/// @return true while the crash-recovery dialog is shown.
[[nodiscard]] bool isRecoveryDialogOpen();

}// namespace StateManagement

#endif// STARTUP_TRACE_HPP
