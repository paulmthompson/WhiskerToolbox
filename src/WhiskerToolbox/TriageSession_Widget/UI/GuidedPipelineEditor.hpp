/**
 * @file GuidedPipelineEditor.hpp
 * @brief Editable command list for building command sequences visually
 */

#ifndef GUIDED_PIPELINE_EDITOR_HPP
#define GUIDED_PIPELINE_EDITOR_HPP

#include "Commands/Core/CommandDescriptor.hpp"

#include <QWidget>

#include <string>
#include <vector>

class CommandRowWidget;
class QPushButton;
class QVBoxLayout;

/**
 * @brief Visual editor for building a CommandSequenceDescriptor
 *
 * Displays an editable list of commands, each rendered as an expandable
 * CommandRowWidget with an AutoParamWidget for parameter editing.
 * Provides "Add Command..." button and reorder/remove controls.
 *
 * Maintains bidirectional sync with a CommandSequenceDescriptor:
 * - toSequence() builds a descriptor from the current UI state
 * - fromSequence() populates the UI from a descriptor
 *
 * Handles ${variable} references: text fields in AutoParamWidget accept
 * both literal values and variable strings like "${mark_frame}".
 */
class GuidedPipelineEditor : public QWidget {
    Q_OBJECT

public:
    explicit GuidedPipelineEditor(QWidget * parent = nullptr);

    /// @brief Build a CommandSequenceDescriptor from current UI state
    [[nodiscard]] commands::CommandSequenceDescriptor toSequence() const;

    /// @brief Populate the editor from an existing CommandSequenceDescriptor
    void fromSequence(commands::CommandSequenceDescriptor const & seq);

    /// @brief Clear all command rows
    void clear();

signals:
    /// @brief Emitted when any command is added, removed, reordered, or has params changed
    void pipelineChanged();

private slots:
    void _onAddCommandClicked();

private:
    void _addCommandRow(std::string const & command_name);
    void _removeRow(CommandRowWidget * row);
    void _moveRowUp(CommandRowWidget * row);
    void _moveRowDown(CommandRowWidget * row);
    void _updateMoveButtons();
    void _onRowChanged();

    QVBoxLayout * _rows_layout = nullptr;
    QPushButton * _add_button = nullptr;

    std::vector<CommandRowWidget *> _rows;
    bool _updating = false;///< Guard against recursive signal emission
};

#endif// GUIDED_PIPELINE_EDITOR_HPP
