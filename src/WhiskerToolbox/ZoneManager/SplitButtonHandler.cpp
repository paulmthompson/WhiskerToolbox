#include "SplitButtonHandler.hpp"

#include "DockAreaTitleBar.h"
#include "DockAreaWidget.h"
#include "DockManager.h"
#include "DockWidget.h"

#include <QApplication>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QStyle>

SplitButtonHandler::SplitButtonHandler(ads::CDockManager * dock_manager, QObject * parent)
    : QObject(parent),
      _dock_manager(dock_manager),
      _split_icon(createDefaultSplitIcon()),
      _tooltip(tr("Split Editor (Ctrl+click for vertical split)")) {
    // Connect to dock area creation signal to add split buttons to new areas
    connect(_dock_manager, &ads::CDockManager::dockAreaCreated,
            this, &SplitButtonHandler::onDockAreaCreated);

    // Add split buttons to any existing dock areas
    // (in case handler is created after some areas already exist)
    for (int i = 0; i < _dock_manager->dockAreaCount(); ++i) {
        auto * area = _dock_manager->dockArea(i);
        if (area) {
            addSplitButtonToArea(area);
        }
    }
}

SplitButtonHandler::~SplitButtonHandler() {
    // Clean up any remaining buttons
    // Qt parent-child relationships will handle actual deletion
    _split_buttons.clear();
}

void SplitButtonHandler::setEnabled(bool enabled) {
    if (_enabled == enabled) {
        return;
    }

    _enabled = enabled;

    // Update visibility of all tracked buttons
    for (auto const & button_ptr: _split_buttons) {
        if (auto * button = button_ptr.data()) {
            button->setVisible(_enabled);
        }
    }

    // Update visibility of all vertical split buttons
    for (auto const & button_ptr: _vertical_split_buttons) {
        if (auto * button = button_ptr.data()) {
            button->setVisible(_enabled);
        }
    }
}

void SplitButtonHandler::setDefaultSplitDirection(SplitDirection direction) {
    _default_direction = direction;
}

void SplitButtonHandler::setSplitIcon(QIcon const & icon) {
    _split_icon = icon;

    // Update all existing buttons
    for (auto const & button_ptr: _split_buttons) {
        if (auto * button = button_ptr.data()) {
            button->setIcon(_split_icon);
        }
    }
}

void SplitButtonHandler::setTooltip(QString const & tooltip) {
    _tooltip = tooltip;

    // Update all existing buttons
    for (auto const & button_ptr: _split_buttons) {
        if (auto * button = button_ptr.data()) {
            button->setToolTip(_tooltip);
        }
    }
}

void SplitButtonHandler::onDockAreaCreated(ads::CDockAreaWidget * dock_area) {
    if (dock_area) {
        addSplitButtonToArea(dock_area);
    }
}

void SplitButtonHandler::onSplitButtonClicked() {
    auto * button = qobject_cast<QToolButton *>(sender());
    if (!button) {
        return;
    }

    // Find the dock area this button belongs to
    // The button's parent chain should lead to the title bar, then the dock area
    QWidget * parent = button->parentWidget();
    while (parent) {
        if (auto * title_bar = qobject_cast<ads::CDockAreaTitleBar *>(parent)) {
            auto * dock_area = title_bar->dockAreaWidget();
            if (dock_area) {
                // Determine split direction based on modifier keys
                auto direction = _default_direction;
                auto modifiers = QApplication::keyboardModifiers();

                if (modifiers & Qt::ControlModifier) {
                    // Ctrl+click inverts the default direction
                    direction = (direction == SplitDirection::Horizontal)
                                        ? SplitDirection::Vertical
                                        : SplitDirection::Horizontal;
                }

                // Emit both signals for flexibility
                emit splitRequested(dock_area, direction);

                // Also emit the dock widget signal with the current widget
                auto * current_widget = dock_area->currentDockWidget();
                if (current_widget) {
                    emit splitDockWidgetRequested(current_widget, direction);
                }
            }
            break;
        }
        parent = parent->parentWidget();
    }
}

void SplitButtonHandler::onVerticalSplitButtonClicked() {
    auto * button = qobject_cast<QToolButton *>(sender());
    if (!button) {
        return;
    }

    // Find the dock area this button belongs to
    QWidget * parent = button->parentWidget();
    while (parent) {
        if (auto * title_bar = qobject_cast<ads::CDockAreaTitleBar *>(parent)) {
            auto * dock_area = title_bar->dockAreaWidget();
            if (dock_area) {
                // Vertical split button always splits vertically (top/bottom)
                auto direction = SplitDirection::Vertical;

                // Emit both signals for flexibility
                emit splitRequested(dock_area, direction);

                // Also emit the dock widget signal with the current widget
                auto * current_widget = dock_area->currentDockWidget();
                if (current_widget) {
                    emit splitDockWidgetRequested(current_widget, direction);
                }
            }
            break;
        }
        parent = parent->parentWidget();
    }
}

QIcon SplitButtonHandler::createDefaultSplitIcon() {
    // Create a simple split icon programmatically
    // This creates a horizontal split icon (two rectangles side by side)
    int const size = 16;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Use the application's palette for theme compatibility
    QColor const color = QApplication::palette().color(QPalette::ButtonText);
    double const pen_width = 1.2;
    painter.setPen(QPen(color, pen_width));
    painter.setBrush(Qt::NoBrush);

    // Draw two rectangles side by side (representing split view)
    int const margin = 2;
    int const gap = 2;
    int const rectWidth = (size - 2 * margin - gap) / 2;
    int const rectHeight = size - 2 * margin;

    // Left rectangle
    painter.drawRect(margin, margin, rectWidth, rectHeight);

    // Right rectangle
    painter.drawRect(margin + rectWidth + gap, margin, rectWidth, rectHeight);

    return {pixmap};
}

QIcon SplitButtonHandler::createDefaultVerticalSplitIcon() {
    // Create a vertical split icon programmatically
    // This creates a vertical split icon (two rectangles stacked top/bottom)
    int const size = 16;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Use the application's palette for theme compatibility
    QColor const color = QApplication::palette().color(QPalette::ButtonText);
    double const pen_width = 1.2;
    painter.setPen(QPen(color, pen_width));
    painter.setBrush(Qt::NoBrush);

    // Draw two rectangles stacked vertically (representing vertical split view)
    int const margin = 2;
    int const gap = 2;
    int const rectWidth = size - 2 * margin;
    int const rectHeight = (size - 2 * margin - gap) / 2;

    // Top rectangle
    painter.drawRect(margin, margin, rectWidth, rectHeight);

    // Bottom rectangle
    painter.drawRect(margin, margin + rectHeight + gap, rectWidth, rectHeight);

    return {pixmap};
}

void SplitButtonHandler::addSplitButtonToArea(ads::CDockAreaWidget * dock_area) {
    if (!dock_area) {
        return;
    }

    auto * title_bar = dock_area->titleBar();
    if (!title_bar) {
        return;
    }

    // Get button size from existing title bar buttons
    int const button_size = title_bar->button(ads::TitleBarButtonClose)
                                    ? title_bar->button(ads::TitleBarButtonClose)->iconSize().width()
                                    : 16;

    // Get the index of the close button for insertion
    auto * close_button = title_bar->button(ads::TitleBarButtonClose);
    int insert_index = -1;// Default to end

    if (close_button) {
        insert_index = title_bar->indexOf(close_button);
    }

    // Create horizontal split button (side by side)
    auto * split_button = new QToolButton(title_bar);
    split_button->setObjectName(QStringLiteral("SplitButton"));
    split_button->setIcon(_split_icon);
    split_button->setToolTip(_tooltip);
    split_button->setAutoRaise(true);
    split_button->setVisible(_enabled);
    split_button->setProperty("showInTitleBar", true);
    split_button->setIconSize(QSize(button_size, button_size));

    // Connect click signal
    connect(split_button, &QToolButton::clicked,
            this, &SplitButtonHandler::onSplitButtonClicked);

    // Insert the horizontal split button before the close button
    title_bar->insertWidget(insert_index, split_button);

    // Track this button for later management
    _split_buttons.append(split_button);

    // Clean up tracking when button is destroyed
    connect(split_button, &QObject::destroyed, this, [this, split_button]() {
        _split_buttons.removeAll(split_button);
    });

    // Create vertical split button (top/bottom)
    auto * vertical_split_button = new QToolButton(title_bar);
    vertical_split_button->setObjectName(QStringLiteral("VerticalSplitButton"));
    vertical_split_button->setIcon(createDefaultVerticalSplitIcon());
    vertical_split_button->setToolTip(tr("Split Editor Vertically (Top/Bottom)"));
    vertical_split_button->setAutoRaise(true);
    vertical_split_button->setVisible(_enabled);
    vertical_split_button->setProperty("showInTitleBar", true);
    vertical_split_button->setIconSize(QSize(button_size, button_size));

    // Connect click signal for vertical split
    connect(vertical_split_button, &QToolButton::clicked,
            this, &SplitButtonHandler::onVerticalSplitButtonClicked);

    // Insert the vertical split button before the horizontal split button
    // (so it appears to the left of the horizontal split button)
    int horizontal_split_index = title_bar->indexOf(split_button);
    title_bar->insertWidget(horizontal_split_index, vertical_split_button);

    // Track this button for later management
    _vertical_split_buttons.append(vertical_split_button);

    // Clean up tracking when button is destroyed
    connect(vertical_split_button, &QObject::destroyed, this, [this, vertical_split_button]() {
        _vertical_split_buttons.removeAll(vertical_split_button);
    });
}
