/**
 * @file CommandLogWidget.hpp
 * @brief Minimal dock widget showing recorded CommandDescriptor entries
 *
 * Displays the list of commands captured by CommandRecorder during the
 * current session. Provides "Copy as JSON" to export the recorded trace
 * as a CommandSequenceDescriptor and "Clear" to reset the recorder.
 *
 * This is intentionally minimal — NOT the full Phase 4 Action Journal.
 * No persistence, timestamps, filtering, or provenance.
 */

#ifndef COMMAND_LOG_WIDGET_HPP
#define COMMAND_LOG_WIDGET_HPP

#include <QWidget>

class QListWidget;
class QPushButton;
class QLabel;

namespace commands {
class CommandRecorder;
}// namespace commands

/**
 * @brief Widget displaying recorded commands from a CommandRecorder
 *
 * Shows command names and parameters in a list. Provides buttons to
 * copy the full trace as JSON or clear the recorder.
 */
class CommandLogWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct the Command Log widget
     * @param recorder Non-owning pointer to the shared CommandRecorder
     * @param parent Parent QWidget
     */
    explicit CommandLogWidget(commands::CommandRecorder * recorder,
                              QWidget * parent = nullptr);

    ~CommandLogWidget() override = default;

    /**
     * @brief Refresh the displayed command list from the recorder
     *
     * Call this after commands are executed to update the display.
     * Also called automatically via a periodic poll timer.
     */
    void refresh();

private slots:
    void _onCopyAsJsonClicked();
    void _onClearClicked();

private:
    commands::CommandRecorder * _recorder;
    QListWidget * _command_list;
    QPushButton * _copy_btn;
    QPushButton * _clear_btn;
    QLabel * _count_label;

    /// Track how many entries we've seen to avoid redundant full refreshes
    size_t _last_known_size = 0;
};

#endif// COMMAND_LOG_WIDGET_HPP
