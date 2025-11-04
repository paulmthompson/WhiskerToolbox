#include "MediaText_Widget.hpp"

#include "ui_MediaText_Widget.h"

#include <QAction>
#include <QColorDialog>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QTableWidgetItem>

#include <algorithm>
#include <iostream>

MediaText_Widget::MediaText_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MediaText_Widget) {
    ui->setupUi(this);

    // Initialize data
    _next_overlay_id = 0;

    // Setup UI components
    _setupTable();
    _setupContextMenu();

    // Connect signals
    connect(ui->add_text_button, &QPushButton::clicked, this, &MediaText_Widget::_onAddTextClicked);
    connect(ui->color_button, &QPushButton::clicked, this, [this]() {
        QColor color = QColorDialog::getColor(Qt::white, this, "Select Text Color");
        if (color.isValid()) {
            QString style = QString("background-color: %1; border: 1px solid black;").arg(color.name());
            ui->color_button->setStyleSheet(style);
        }
    });
    connect(ui->clear_all_button, &QPushButton::clicked, this, [this]() {
        if (!_text_overlays.empty()) {
            auto reply = QMessageBox::question(this, "Clear All Text Overlays",
                                               "Are you sure you want to remove all text overlays?",
                                               QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                clearAllTextOverlays();
            }
        }
    });

    connect(ui->overlays_table, &QTableWidget::itemChanged, this, &MediaText_Widget::_onTableItemChanged);
    connect(ui->overlays_table, &QTableWidget::customContextMenuRequested, this, &MediaText_Widget::_onTableContextMenu);

    // Update count label
    ui->count_label->setText("Total: 0 overlays");
}

MediaText_Widget::~MediaText_Widget() {
    delete ui;
}

void MediaText_Widget::addTextOverlay(TextOverlay const & overlay) {
    TextOverlay new_overlay = overlay;
    new_overlay.id = _next_overlay_id++;

    _text_overlays.push_back(new_overlay);

    // Add to table
    int row = ui->overlays_table->rowCount();
    ui->overlays_table->insertRow(row);
    _populateTableRow(row, new_overlay);

    // Update count
    ui->count_label->setText(QString("Total: %1 overlays").arg(_text_overlays.size()));

    emit textOverlayAdded(new_overlay);

    std::cout << "Added text overlay: \"" << new_overlay.text.toStdString()
              << "\" at (" << new_overlay.x_position << ", " << new_overlay.y_position << ")" << std::endl;
}

void MediaText_Widget::removeTextOverlay(int overlay_id) {
    auto it = std::find_if(_text_overlays.begin(), _text_overlays.end(),
                           [overlay_id](TextOverlay const & overlay) {
                               return overlay.id == overlay_id;
                           });

    if (it != _text_overlays.end()) {
        _text_overlays.erase(it);
        refreshTable();

        // Update count
        ui->count_label->setText(QString("Total: %1 overlays").arg(_text_overlays.size()));

        emit textOverlayRemoved(overlay_id);

        std::cout << "Removed text overlay with ID: " << overlay_id << std::endl;
    }
}

void MediaText_Widget::updateTextOverlay(int overlay_id, TextOverlay const & updated_overlay) {
    auto it = std::find_if(_text_overlays.begin(), _text_overlays.end(),
                           [overlay_id](TextOverlay const & overlay) {
                               return overlay.id == overlay_id;
                           });

    if (it != _text_overlays.end()) {
        TextOverlay updated = updated_overlay;
        updated.id = overlay_id;// Preserve ID
        *it = updated;

        // Find and update the corresponding table row
        int row = _findOverlayRowById(overlay_id);
        if (row >= 0) {
            _updateTableRow(row, updated);
        }

        emit textOverlayUpdated(overlay_id, updated);

        std::cout << "Updated text overlay with ID: " << overlay_id << std::endl;
    }
}

void MediaText_Widget::clearAllTextOverlays() {
    _text_overlays.clear();
    ui->overlays_table->setRowCount(0);

    // Update count
    ui->count_label->setText("Total: 0 overlays");

    emit textOverlaysCleared();

    std::cout << "Cleared all text overlays" << std::endl;
}

std::vector<TextOverlay> MediaText_Widget::getEnabledTextOverlays() const {
    std::vector<TextOverlay> enabled_overlays;

    for (auto const & overlay: _text_overlays) {
        if (overlay.enabled) {
            enabled_overlays.push_back(overlay);
        }
    }

    return enabled_overlays;
}

void MediaText_Widget::refreshTable() {
    ui->overlays_table->setRowCount(0);

    for (auto const & overlay: _text_overlays) {
        int row = ui->overlays_table->rowCount();
        ui->overlays_table->insertRow(row);
        _populateTableRow(row, overlay);
    }

    // Update count
    ui->count_label->setText(QString("Total: %1 overlays").arg(_text_overlays.size()));
}

void MediaText_Widget::_onAddTextClicked() {
    QString text = ui->text_input->text().trimmed();
    if (text.isEmpty()) {
        QMessageBox::warning(this, "Invalid Input", "Please enter some text to display.");
        return;
    }

    // Get color from button style
    QString style = ui->color_button->styleSheet();
    QColor color = Qt::white;
    if (style.contains("background-color:")) {
        QString color_name = style.split("background-color:")[1].split(";")[0].trimmed();
        color = QColor(color_name);
    }

    TextOrientation orientation = (ui->orientation_combo->currentIndex() == 0) ? TextOrientation::Horizontal : TextOrientation::Vertical;

    TextOverlay overlay(
            text,
            orientation,
            static_cast<float>(ui->x_position_spinbox->value()),
            static_cast<float>(ui->y_position_spinbox->value()),
            color,
            ui->font_size_spinbox->value(),
            true);

    addTextOverlay(overlay);

    // Clear input for next entry
    ui->text_input->clear();
}

void MediaText_Widget::_onTableItemChanged(QTableWidgetItem * item) {
    if (!item) return;

    int row = item->row();
    int column = item->column();

    if (row < 0 || row >= static_cast<int>(_text_overlays.size())) return;

    TextOverlay & overlay = _text_overlays[row];

    // Update overlay based on which column was changed
    switch (column) {
        case 0:// Enabled
            overlay.enabled = (item->checkState() == Qt::Checked);
            break;
        case 1:// Text
            overlay.text = item->text();
            break;
        case 2:// Orientation
            overlay.orientation = _stringToOrientation(item->text());
            break;
        case 3:// X Position
        {
            bool ok;
            float x = item->text().toFloat(&ok);
            if (ok && x >= 0.0f && x <= 1.0f) {
                overlay.x_position = x;
            } else {
                // Reset to previous value if invalid
                item->setText(QString::number(overlay.x_position, 'f', 3));
            }
        } break;
        case 4:// Y Position
        {
            bool ok;
            float y = item->text().toFloat(&ok);
            if (ok && y >= 0.0f && y <= 1.0f) {
                overlay.y_position = y;
            } else {
                // Reset to previous value if invalid
                item->setText(QString::number(overlay.y_position, 'f', 3));
            }
        } break;
        case 5:// Color
        {
            QColor color = _stringToColor(item->text());
            if (color.isValid()) {
                overlay.color = color;
            } else {
                // Reset to previous value if invalid
                item->setText(_colorToString(overlay.color));
            }
        } break;
        case 6:// Font Size
        {
            bool ok;
            int size = item->text().toInt(&ok);
            if (ok && size >= 8 && size <= 72) {
                overlay.font_size = size;
            } else {
                // Reset to previous value if invalid
                item->setText(QString::number(overlay.font_size));
            }
        } break;
    }

    emit textOverlayUpdated(overlay.id, overlay);
}

void MediaText_Widget::_onTableContextMenu(QPoint const & position) {
    QTableWidgetItem * item = ui->overlays_table->itemAt(position);
    if (!item) return;

    int selected_id = _getSelectedOverlayId();
    if (selected_id < 0) return;

    // Update context menu state
    auto it = std::find_if(_text_overlays.begin(), _text_overlays.end(),
                           [selected_id](TextOverlay const & overlay) {
                               return overlay.id == selected_id;
                           });

    if (it != _text_overlays.end()) {
        _toggle_enabled_action->setText(it->enabled ? "Disable" : "Enable");
        _context_menu->exec(ui->overlays_table->mapToGlobal(position));
    }
}

void MediaText_Widget::_onDeleteSelectedOverlay() {
    int selected_id = _getSelectedOverlayId();
    if (selected_id >= 0) {
        removeTextOverlay(selected_id);
    }
}

void MediaText_Widget::_onToggleOverlayEnabled() {
    int selected_id = _getSelectedOverlayId();
    if (selected_id < 0) return;

    auto it = std::find_if(_text_overlays.begin(), _text_overlays.end(),
                           [selected_id](TextOverlay const & overlay) {
                               return overlay.id == selected_id;
                           });

    if (it != _text_overlays.end()) {
        it->enabled = !it->enabled;

        // Update table
        int row = _findOverlayRowById(selected_id);
        if (row >= 0) {
            QTableWidgetItem * enabled_item = ui->overlays_table->item(row, 0);
            if (enabled_item) {
                enabled_item->setCheckState(it->enabled ? Qt::Checked : Qt::Unchecked);
            }
        }

        emit textOverlayUpdated(selected_id, *it);
    }
}

void MediaText_Widget::_onEditSelectedOverlay() {
    int selected_id = _getSelectedOverlayId();
    if (selected_id < 0) return;

    auto it = std::find_if(_text_overlays.begin(), _text_overlays.end(),
                           [selected_id](TextOverlay const & overlay) {
                               return overlay.id == selected_id;
                           });

    if (it != _text_overlays.end()) {
        // Populate form with selected overlay data
        ui->text_input->setText(it->text);
        ui->orientation_combo->setCurrentIndex(it->orientation == TextOrientation::Horizontal ? 0 : 1);
        ui->x_position_spinbox->setValue(it->x_position);
        ui->y_position_spinbox->setValue(it->y_position);
        ui->font_size_spinbox->setValue(it->font_size);

        // Update color button
        QString style = QString("background-color: %1; border: 1px solid black;").arg(it->color.name());
        ui->color_button->setStyleSheet(style);

        std::cout << "Loaded overlay into form for editing: " << it->text.toStdString() << std::endl;
    }
}

void MediaText_Widget::_setupTable() {
    ui->overlays_table->setColumnCount(7);
    ui->overlays_table->setHorizontalHeaderLabels({"Enabled", "Text", "Orientation", "X Position", "Y Position", "Color", "Font Size"});

    // Set column widths
    ui->overlays_table->horizontalHeader()->setStretchLastSection(false);
    ui->overlays_table->setColumnWidth(0, 60); // Enabled
    ui->overlays_table->setColumnWidth(1, 120);// Text
    ui->overlays_table->setColumnWidth(2, 80); // Orientation
    ui->overlays_table->setColumnWidth(3, 80); // X Position
    ui->overlays_table->setColumnWidth(4, 80); // Y Position
    ui->overlays_table->setColumnWidth(5, 80); // Color
    ui->overlays_table->setColumnWidth(6, 70); // Font Size

    // Allow resizing
    ui->overlays_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    ui->overlays_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

void MediaText_Widget::_setupContextMenu() {
    _context_menu = std::make_unique<QMenu>(this);

    _edit_action = new QAction("Edit", this);
    _toggle_enabled_action = new QAction("Toggle Enabled", this);
    _delete_action = new QAction("Delete", this);

    connect(_edit_action, &QAction::triggered, this, &MediaText_Widget::_onEditSelectedOverlay);
    connect(_toggle_enabled_action, &QAction::triggered, this, &MediaText_Widget::_onToggleOverlayEnabled);
    connect(_delete_action, &QAction::triggered, this, &MediaText_Widget::_onDeleteSelectedOverlay);

    _context_menu->addAction(_edit_action);
    _context_menu->addAction(_toggle_enabled_action);
    _context_menu->addSeparator();
    _context_menu->addAction(_delete_action);
}

void MediaText_Widget::_populateTableRow(int row, TextOverlay const & overlay) {
    // Enabled checkbox
    auto * enabled_item = new QTableWidgetItem();
    enabled_item->setCheckState(overlay.enabled ? Qt::Checked : Qt::Unchecked);
    enabled_item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    ui->overlays_table->setItem(row, 0, enabled_item);

    // Text
    ui->overlays_table->setItem(row, 1, new QTableWidgetItem(overlay.text));

    // Orientation
    ui->overlays_table->setItem(row, 2, new QTableWidgetItem(_orientationToString(overlay.orientation)));

    // X Position
    ui->overlays_table->setItem(row, 3, new QTableWidgetItem(QString::number(overlay.x_position, 'f', 3)));

    // Y Position
    ui->overlays_table->setItem(row, 4, new QTableWidgetItem(QString::number(overlay.y_position, 'f', 3)));

    // Color
    ui->overlays_table->setItem(row, 5, new QTableWidgetItem(_colorToString(overlay.color)));

    // Font Size
    ui->overlays_table->setItem(row, 6, new QTableWidgetItem(QString::number(overlay.font_size)));

    // Store overlay ID in the first item for reference
    enabled_item->setData(Qt::UserRole, overlay.id);
}

void MediaText_Widget::_updateTableRow(int row, TextOverlay const & overlay) {
    if (row < 0 || row >= ui->overlays_table->rowCount()) return;

    // Update all cells
    ui->overlays_table->item(row, 0)->setCheckState(overlay.enabled ? Qt::Checked : Qt::Unchecked);
    ui->overlays_table->item(row, 1)->setText(overlay.text);
    ui->overlays_table->item(row, 2)->setText(_orientationToString(overlay.orientation));
    ui->overlays_table->item(row, 3)->setText(QString::number(overlay.x_position, 'f', 3));
    ui->overlays_table->item(row, 4)->setText(QString::number(overlay.y_position, 'f', 3));
    ui->overlays_table->item(row, 5)->setText(_colorToString(overlay.color));
    ui->overlays_table->item(row, 6)->setText(QString::number(overlay.font_size));
}

int MediaText_Widget::_findOverlayRowById(int overlay_id) const {
    for (int row = 0; row < ui->overlays_table->rowCount(); ++row) {
        QTableWidgetItem * item = ui->overlays_table->item(row, 0);
        if (item && item->data(Qt::UserRole).toInt() == overlay_id) {
            return row;
        }
    }
    return -1;
}

int MediaText_Widget::_getSelectedOverlayId() const {
    int current_row = ui->overlays_table->currentRow();
    if (current_row < 0 || current_row >= ui->overlays_table->rowCount()) {
        return -1;
    }

    QTableWidgetItem * item = ui->overlays_table->item(current_row, 0);
    if (item) {
        return item->data(Qt::UserRole).toInt();
    }

    return -1;
}

QString MediaText_Widget::_orientationToString(TextOrientation orientation) const {
    switch (orientation) {
        case TextOrientation::Horizontal:
            return "Horizontal";
        case TextOrientation::Vertical:
            return "Vertical";
        default:
            return "Horizontal";
    }
}

TextOrientation MediaText_Widget::_stringToOrientation(QString const & str) const {
    if (str.toLower() == "vertical") {
        return TextOrientation::Vertical;
    }
    return TextOrientation::Horizontal;
}

QString MediaText_Widget::_colorToString(QColor const & color) const {
    return color.name();
}

QColor MediaText_Widget::_stringToColor(QString const & str) const {
    QColor color(str);
    return color.isValid() ? color : Qt::white;
}