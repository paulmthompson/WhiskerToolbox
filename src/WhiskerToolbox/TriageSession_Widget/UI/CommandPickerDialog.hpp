/**
 * @file CommandPickerDialog.hpp
 * @brief Dialog for selecting a command from the command factory registry
 */

#ifndef COMMAND_PICKER_DIALOG_HPP
#define COMMAND_PICKER_DIALOG_HPP

#include <QDialog>

#include <optional>
#include <string>

class QComboBox;
class QLabel;

namespace commands {
struct CommandInfo;
}

/**
 * @brief Modal dialog that lists available commands and lets the user pick one
 *
 * Populated by commands::getAvailableCommands(). Shows the command description
 * when a command is selected. Returns the chosen command name on accept.
 */
class CommandPickerDialog : public QDialog {
    Q_OBJECT

public:
    explicit CommandPickerDialog(QWidget * parent = nullptr);

    /// @brief Returns the selected command name, or nullopt if cancelled
    [[nodiscard]] std::optional<std::string> selectedCommandName() const;

private slots:
    void _onSelectionChanged(int index);

private:
    void _buildUI();
    void _populateCommands();

    QComboBox * _command_combo = nullptr;
    QLabel * _description_label = nullptr;
    QLabel * _category_label = nullptr;

    std::vector<commands::CommandInfo> _commands;
};

#endif// COMMAND_PICKER_DIALOG_HPP
