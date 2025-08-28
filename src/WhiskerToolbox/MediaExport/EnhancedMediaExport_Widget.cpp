#include "EnhancedMediaExport_Widget.hpp"

#include "../Media_Window/Media_Window.hpp"
#include "../MediaDisplay/MediaDisplayCoordinator.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QGroupBox>
#include <QImage>
#include <QPainter>
#include <QFileDialog>
#include <QMessageBox>

#include <iostream>

EnhancedMediaExport_Widget::EnhancedMediaExport_Widget(MediaDisplayCoordinator* coordinator, 
                                                       QWidget *parent)
    : QWidget(parent), _coordinator(coordinator), _export_layout(ExportLayout::Individual) {
    _setupUI();
    _refreshDisplayList();
    
    // Connect to coordinator signals to update display list
    connect(_coordinator, &MediaDisplayCoordinator::displayCreated,
            this, &EnhancedMediaExport_Widget::_refreshDisplayList);
    
    connect(_coordinator, &MediaDisplayCoordinator::displayRemoved,
            this, &EnhancedMediaExport_Widget::_refreshDisplayList);
}

EnhancedMediaExport_Widget::~EnhancedMediaExport_Widget() = default;

void EnhancedMediaExport_Widget::_setupUI() {
    auto* main_layout = new QVBoxLayout(this);
    
    // Display selection group
    auto* display_group = new QGroupBox("Select Displays to Export", this);
    auto* display_layout = new QVBoxLayout(display_group);
    
    // Scroll area for display checkboxes
    auto* scroll_area = new QScrollArea(this);
    auto* scroll_widget = new QWidget();
    auto* scroll_layout = new QVBoxLayout(scroll_widget);
    scroll_layout->setObjectName("display_checkboxes_layout");
    
    scroll_area->setWidget(scroll_widget);
    scroll_area->setWidgetResizable(true);
    scroll_area->setMaximumHeight(200);
    
    display_layout->addWidget(scroll_area);
    
    // Layout mode selection
    auto* layout_group = new QGroupBox("Export Layout", this);
    auto* layout_layout = new QVBoxLayout(layout_group);
    
    auto* layout_combo = new QComboBox(this);
    layout_combo->addItem("Individual Images", static_cast<int>(ExportLayout::Individual));
    layout_combo->addItem("Horizontal Concatenation", static_cast<int>(ExportLayout::HorizontalConcat));
    layout_combo->addItem("Vertical Concatenation", static_cast<int>(ExportLayout::VerticalConcat));
    layout_combo->setObjectName("layout_combo");
    
    connect(layout_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EnhancedMediaExport_Widget::_onLayoutModeChanged);
    
    layout_layout->addWidget(new QLabel("Layout Mode:"));
    layout_layout->addWidget(layout_combo);
    
    // Buttons
    auto* button_layout = new QHBoxLayout();
    auto* preview_button = new QPushButton("Preview Export", this);
    auto* export_button = new QPushButton("Export", this);
    
    connect(preview_button, &QPushButton::clicked, this, &EnhancedMediaExport_Widget::previewExport);
    connect(export_button, &QPushButton::clicked, this, &EnhancedMediaExport_Widget::exportMedia);
    
    button_layout->addWidget(preview_button);
    button_layout->addWidget(export_button);
    
    // Add all to main layout
    main_layout->addWidget(display_group);
    main_layout->addWidget(layout_group);
    main_layout->addLayout(button_layout);
    main_layout->addStretch();
}

void EnhancedMediaExport_Widget::_updateDisplayCheckboxes() {
    // Find the scroll layout
    auto* scroll_layout = findChild<QVBoxLayout*>("display_checkboxes_layout");
    if (!scroll_layout) return;
    
    // Clear existing checkboxes
    QLayoutItem* item;
    while ((item = scroll_layout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    
    // Add checkboxes for each display
    auto displays = _coordinator->getActiveDisplays();
    for (const auto* display : displays) {
        auto* checkbox = new QCheckBox(QString::fromStdString(display->getId()), this);
        
        // Check if this display is in selected list
        auto it = std::find(_selected_display_ids.begin(), _selected_display_ids.end(), display->getId());
        checkbox->setChecked(it != _selected_display_ids.end());
        
        connect(checkbox, &QCheckBox::toggled, this, &EnhancedMediaExport_Widget::_onDisplaySelectionChanged);
        
        scroll_layout->addWidget(checkbox);
    }
}

void EnhancedMediaExport_Widget::_refreshDisplayList() {
    _updateDisplayCheckboxes();
}

void EnhancedMediaExport_Widget::_onLayoutModeChanged() {
    auto* combo = findChild<QComboBox*>("layout_combo");
    if (combo) {
        _export_layout = static_cast<ExportLayout>(combo->currentData().toInt());
    }
}

void EnhancedMediaExport_Widget::_onDisplaySelectionChanged() {
    _selected_display_ids.clear();
    
    auto* scroll_layout = findChild<QVBoxLayout*>("display_checkboxes_layout");
    if (!scroll_layout) return;
    
    for (int i = 0; i < scroll_layout->count(); ++i) {
        auto* checkbox = qobject_cast<QCheckBox*>(scroll_layout->itemAt(i)->widget());
        if (checkbox && checkbox->isChecked()) {
            _selected_display_ids.push_back(checkbox->text().toStdString());
        }
    }
}

void EnhancedMediaExport_Widget::setSelectedDisplays(const std::vector<std::string>& display_ids) {
    _selected_display_ids = display_ids;
    _updateDisplayCheckboxes();
}

void EnhancedMediaExport_Widget::setExportLayout(ExportLayout layout) {
    _export_layout = layout;
    auto* combo = findChild<QComboBox*>("layout_combo");
    if (combo) {
        combo->setCurrentIndex(static_cast<int>(layout));
    }
}

void EnhancedMediaExport_Widget::previewExport() {
    if (_selected_display_ids.empty()) {
        QMessageBox::warning(this, "No Displays Selected", "Please select at least one display to preview.");
        return;
    }
    
    auto scenes = _coordinator->getSelectedScenesForExport(_selected_display_ids);
    if (scenes.empty()) {
        QMessageBox::warning(this, "No Valid Scenes", "No valid scenes found for the selected displays.");
        return;
    }
    
    // For now, just show a message with export info
    QString info = QString("Would export %1 display(s) with %2 layout")
                   .arg(_selected_display_ids.size())
                   .arg(_export_layout == ExportLayout::Individual ? "individual" : 
                        _export_layout == ExportLayout::HorizontalConcat ? "horizontal" : "vertical");
    
    QMessageBox::information(this, "Export Preview", info);
}

void EnhancedMediaExport_Widget::exportMedia() {
    if (_selected_display_ids.empty()) {
        QMessageBox::warning(this, "No Displays Selected", "Please select at least one display to export.");
        return;
    }
    
    // Get file save location
    QString fileName = QFileDialog::getSaveFileName(this, "Save Export", "", "PNG Images (*.png);;JPEG Images (*.jpg)");
    if (fileName.isEmpty()) return;
    
    auto scenes = _coordinator->getSelectedScenesForExport(_selected_display_ids);
    if (scenes.empty()) {
        QMessageBox::warning(this, "No Valid Scenes", "No valid scenes found for the selected displays.");
        return;
    }
    
    try {
        QImage result = _createCompositeImage(scenes);
        if (result.save(fileName)) {
            QMessageBox::information(this, "Export Complete", "Image exported successfully!");
        } else {
            QMessageBox::warning(this, "Export Failed", "Failed to save the exported image.");
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Export Error", QString("Export failed: %1").arg(e.what()));
    }
}

QImage EnhancedMediaExport_Widget::_createCompositeImage(const std::vector<Media_Window*>& scenes) {
    if (scenes.empty()) {
        return QImage();
    }
    
    // Render each scene to an image
    std::vector<QImage> images;
    for (auto* scene : scenes) {
        QImage image(scene->sceneRect().size().toSize(), QImage::Format_ARGB32);
        image.fill(Qt::white);
        
        QPainter painter(&image);
        scene->render(&painter);
        painter.end();
        
        images.push_back(image);
    }
    
    // Combine according to layout mode
    switch (_export_layout) {
        case ExportLayout::Individual:
            // For individual, just return the first image (could be enhanced to save all)
            return images.empty() ? QImage() : images[0];
        case ExportLayout::HorizontalConcat:
            return _concatenateHorizontally(images);
        case ExportLayout::VerticalConcat:
            return _concatenateVertically(images);
        default:
            return images.empty() ? QImage() : images[0];
    }
}

QImage EnhancedMediaExport_Widget::_concatenateHorizontally(const std::vector<QImage>& images) {
    if (images.empty()) return QImage();
    if (images.size() == 1) return images[0];
    
    int total_width = 0;
    int max_height = 0;
    
    for (const auto& img : images) {
        total_width += img.width();
        max_height = std::max(max_height, img.height());
    }
    
    QImage result(total_width, max_height, QImage::Format_ARGB32);
    result.fill(Qt::white);
    
    QPainter painter(&result);
    int x_offset = 0;
    
    for (const auto& img : images) {
        painter.drawImage(x_offset, 0, img);
        x_offset += img.width();
    }
    
    painter.end();
    return result;
}

QImage EnhancedMediaExport_Widget::_concatenateVertically(const std::vector<QImage>& images) {
    if (images.empty()) return QImage();
    if (images.size() == 1) return images[0];
    
    int max_width = 0;
    int total_height = 0;
    
    for (const auto& img : images) {
        max_width = std::max(max_width, img.width());
        total_height += img.height();
    }
    
    QImage result(max_width, total_height, QImage::Format_ARGB32);
    result.fill(Qt::white);
    
    QPainter painter(&result);
    int y_offset = 0;
    
    for (const auto& img : images) {
        painter.drawImage(0, y_offset, img);
        y_offset += img.height();
    }
    
    painter.end();
    return result;
}
