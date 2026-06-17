/**
 * @file CommandRowWidget.hpp
 * @brief Expandable row widget for a single command in the guided pipeline editor
 */

#ifndef COMMAND_ROW_WIDGET_HPP
#define COMMAND_ROW_WIDGET_HPP

#include "Commands/Core/CommandDescriptor.hpp"
#include "Commands/Core/CommandInfo.hpp"

#include <QWidget>

#include <memory>
#include <optional>
#include <string>

class AutoParamWidget;
class DataManager;
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
    ~CommandRowWidget() override;

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

    /**
     * @brief Bind a DataManager and refresh dynamic combo fields
     * @param dm Shared DataManager used to populate `dynamic_combo` parameters
     */
    void setDataManager(std::shared_ptr<DataManager> dm);

    /// @brief Re-populate all `dynamic_combo` fields from the bound DataManager
    void refreshDynamicCombos();

signals:
    void removeRequested();
    void moveUpRequested();
    void moveDownRequested();
    void parametersChanged();

private:
    void _buildUI();
    void _toggleExpanded();
    void _registerDataManagerObserver();

    commands::CommandInfo _info;
    std::shared_ptr<DataManager> _data_manager;
    int _dm_observer_id = -1;
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
