/**
 * @file CommandRowWidget.hpp
 * @brief Expandable row widget for a single command in the guided pipeline editor
 */

#ifndef COMMAND_ROW_WIDGET_HPP
#define COMMAND_ROW_WIDGET_HPP

#include "DataManager/Commands/CommandDescriptor.hpp"
#include "DataManager/Commands/CommandInfo.hpp"

#include <QWidget>

#include <optional>
#include <string>

class AutoParamWidget;
class QLabel;
class QPushButton;
class QToolButton;

/**
 * @brief Expandable row showing a command name and its parameter editor
 *
 * Each row displays the command name and description in a header bar,
 * with expand/collapse to show/hide an AutoParamWidget for parameter editing.
 * Provides remove and move-up/move-down buttons.
 */
class CommandRowWidget : public QWidget {
    Q_OBJECT

public:
    explicit CommandRowWidget(commands::CommandInfo info,
                              QWidget * parent = nullptr);

    /// @brief Get the command name
    [[nodiscard]] std::string commandName() const;

    /// @brief Build a CommandDescriptor from current widget state
    [[nodiscard]] commands::CommandDescriptor toDescriptor() const;

    /// @brief Populate the parameter widgets from a CommandDescriptor
    void fromDescriptor(commands::CommandDescriptor const & desc);

    /// @brief Enable/disable the move-up button
    void setMoveUpEnabled(bool enabled);

    /// @brief Enable/disable the move-down button
    void setMoveDownEnabled(bool enabled);

signals:
    void removeRequested();
    void moveUpRequested();
    void moveDownRequested();
    void parametersChanged();

private:
    void _buildUI();
    void _toggleExpanded();

    commands::CommandInfo _info;
    bool _expanded = false;

    // Header bar
    QToolButton * _expand_button = nullptr;
    QLabel * _name_label = nullptr;
    QPushButton * _remove_button = nullptr;
    QToolButton * _move_up_button = nullptr;
    QToolButton * _move_down_button = nullptr;

    // Content (shown when expanded)
    QWidget * _content_widget = nullptr;
    AutoParamWidget * _param_widget = nullptr;
};

#endif// COMMAND_ROW_WIDGET_HPP
