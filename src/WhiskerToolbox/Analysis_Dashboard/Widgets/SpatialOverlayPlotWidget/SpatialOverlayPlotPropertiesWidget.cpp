#include "SpatialOverlayPlotPropertiesWidget.hpp"
#include "ui_SpatialOverlayPlotPropertiesWidget.h"

#include "SpatialOverlayOpenGLWidget.hpp"
#include "SpatialOverlayPlotWidget.hpp"


#include "DataManager/DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QTimer>

SpatialOverlayPlotPropertiesWidget::SpatialOverlayPlotPropertiesWidget(QWidget * parent)
    : AbstractPlotPropertiesWidget(parent),
      ui(new Ui::SpatialOverlayPlotPropertiesWidget),
      _spatial_plot_widget(nullptr) {
    ui->setupUi(this);
    
    setupConnections();
}

SpatialOverlayPlotPropertiesWidget::~SpatialOverlayPlotPropertiesWidget() {
    delete ui;
}

void SpatialOverlayPlotPropertiesWidget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = std::move(data_manager);
}

void SpatialOverlayPlotPropertiesWidget::setPlotWidget(AbstractPlotWidget * plot_widget) {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: setPlotWidget called with plot widget:" << (plot_widget != nullptr);

    _spatial_plot_widget = qobject_cast<SpatialOverlayPlotWidget *>(plot_widget);
    qDebug() << "SpatialOverlayPlotPropertiesWidget: Cast to SpatialOverlayPlotWidget:" << (_spatial_plot_widget != nullptr);

    if (_spatial_plot_widget) {
        qDebug() << "SpatialOverlayPlotPropertiesWidget: Updating available data sources and UI";
        
        // Connect to plot widget signals
        connect(_spatial_plot_widget, &SpatialOverlayPlotWidget::selectionChanged,
                this, [this](size_t selectedCount) {
            qDebug() << "SpatialOverlayPlotPropertiesWidget: Selection changed, count:" << selectedCount;
            // Could update UI to show selection count if desired
        });
        
        connect(_spatial_plot_widget, &SpatialOverlayPlotWidget::selectionModeChanged,
                this, [this](SelectionMode mode) {
            qDebug() << "SpatialOverlayPlotPropertiesWidget: Selection mode changed to:" << static_cast<int>(mode);
            // Update combo box if changed externally
            for (int i = 0; i < ui->selection_mode_combo->count(); ++i) {
                if (static_cast<SelectionMode>(ui->selection_mode_combo->itemData(i).toInt()) == mode) {
                    ui->selection_mode_combo->blockSignals(true);
                    ui->selection_mode_combo->setCurrentIndex(i);
                    ui->selection_mode_combo->blockSignals(false);
                    ui->clear_selection_button->setEnabled(mode != SelectionMode::None);
                    updateSelectionInstructions();
                    break;
                }
            }
        });
        
        // Update available data sources
        updateAvailableDataSources();
        
        // Update UI from plot
        updateFromPlot();
        
        // Initialize selection instructions
        updateSelectionInstructions();
    }
}

void SpatialOverlayPlotPropertiesWidget::updateFromPlot() {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: updateFromPlot called";
    if (_spatial_plot_widget) {
        // Update UI from current plot state
        QStringList current_keys = _spatial_plot_widget->getPointDataKeys();
        qDebug() << "SpatialOverlayPlotPropertiesWidget: updateFromPlot - current keys from plot:" << current_keys;
        setSelectedDataSources(current_keys);
        
        // Update zoom level and point size from current widget state
        if (_spatial_plot_widget->getOpenGLWidget()) {

            float current_zoom = _spatial_plot_widget->getOpenGLWidget()->getZoomLevel();
            ui->zoom_level_spinbox->blockSignals(true);
            ui->zoom_level_spinbox->setValue(static_cast<double>(current_zoom));
            ui->zoom_level_spinbox->blockSignals(false);

            float current_point_size = _spatial_plot_widget->getOpenGLWidget()->getPointSize();
            ui->point_size_spinbox->blockSignals(true);
            ui->point_size_spinbox->setValue(static_cast<double>(current_point_size));
            ui->point_size_spinbox->blockSignals(false);

            bool tooltips_enabled = _spatial_plot_widget->getOpenGLWidget()->getTooltipsEnabled();
            ui->tooltips_checkbox->blockSignals(true);
            ui->tooltips_checkbox->setChecked(tooltips_enabled);
            ui->tooltips_checkbox->blockSignals(false);

            SelectionMode current_selection_mode = _spatial_plot_widget->getSelectionMode();     
            ui->selection_mode_combo->blockSignals(true);
            for (int i = 0; i < ui->selection_mode_combo->count(); ++i) {
                if (static_cast<SelectionMode>(ui->selection_mode_combo->itemData(i).toInt()) == current_selection_mode) {
                    ui->selection_mode_combo->setCurrentIndex(i);
                    break;
                }
            }
            ui->clear_selection_button->setEnabled(current_selection_mode != SelectionMode::None);
            ui->selection_mode_combo->blockSignals(false);
            updateSelectionInstructions();

        }
    } else {
        qDebug() << "SpatialOverlayPlotPropertiesWidget: updateFromPlot - no spatial plot widget available";
    }
}

void SpatialOverlayPlotPropertiesWidget::applyToPlot() {
    updatePlotWidget();
}

void SpatialOverlayPlotPropertiesWidget::updateAvailableDataSources() {
    refreshDataSourcesList();
}

void SpatialOverlayPlotPropertiesWidget::onDataSourceItemChanged(QListWidgetItem * item) {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: onDataSourceItemChanged called for item:" << item->text() 
             << "checked:" << (item->checkState() == Qt::Checked);
    updatePlotWidget();
}

void SpatialOverlayPlotPropertiesWidget::onPointSizeChanged(double value) {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: onPointSizeChanged called with value:" << value;
    if (_spatial_plot_widget && _spatial_plot_widget->getOpenGLWidget()) {
        _spatial_plot_widget->getOpenGLWidget()->setPointSize(static_cast<float>(value));
    }
}

void SpatialOverlayPlotPropertiesWidget::onZoomLevelChanged(double value) {
    if (_spatial_plot_widget && _spatial_plot_widget->getOpenGLWidget()) {
        _spatial_plot_widget->getOpenGLWidget()->setZoomLevel(static_cast<float>(value));
    }
}

void SpatialOverlayPlotPropertiesWidget::onResetViewClicked() {
    if (_spatial_plot_widget && _spatial_plot_widget->getOpenGLWidget()) {
        // Reset zoom and pan to defaults
        _spatial_plot_widget->getOpenGLWidget()->setZoomLevel(1.0f);
        _spatial_plot_widget->getOpenGLWidget()->setPanOffset(0.0f, 0.0f);
        ui->zoom_level_spinbox->setValue(1.0);
    }
}

void SpatialOverlayPlotPropertiesWidget::onTooltipsEnabledChanged(bool enabled) {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: onTooltipsEnabledChanged called with enabled:" << enabled;
    if (_spatial_plot_widget && _spatial_plot_widget->getOpenGLWidget()) {
        _spatial_plot_widget->getOpenGLWidget()->setTooltipsEnabled(enabled);
    }
}

void SpatialOverlayPlotPropertiesWidget::onSelectAllClicked() {
    for (int i = 0; i < ui->data_sources_list->count(); ++i) {
        QListWidgetItem * item = ui->data_sources_list->item(i);
        item->setCheckState(Qt::Checked);
    }
    updatePlotWidget();
}

void SpatialOverlayPlotPropertiesWidget::onDeselectAllClicked() {
    for (int i = 0; i < ui->data_sources_list->count(); ++i) {
        QListWidgetItem * item = ui->data_sources_list->item(i);
        item->setCheckState(Qt::Unchecked);
    }
    updatePlotWidget();
}

void SpatialOverlayPlotPropertiesWidget::setupConnections() {
       
    // Data source selection
    connect(ui->data_sources_list, &QListWidget::itemChanged,
            this, &SpatialOverlayPlotPropertiesWidget::onDataSourceItemChanged);

    connect(ui->select_all_button, &QPushButton::clicked,
            this, &SpatialOverlayPlotPropertiesWidget::onSelectAllClicked);

    connect(ui->deselect_all_button, &QPushButton::clicked,
            this, &SpatialOverlayPlotPropertiesWidget::onDeselectAllClicked);

    // Visualization settings
    connect(ui->point_size_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SpatialOverlayPlotPropertiesWidget::onPointSizeChanged);

    connect(ui->zoom_level_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SpatialOverlayPlotPropertiesWidget::onZoomLevelChanged);

    connect(ui->reset_view_button, &QPushButton::clicked,
            this, &SpatialOverlayPlotPropertiesWidget::onResetViewClicked);

    // Tooltips checkbox
    connect(ui->tooltips_checkbox, &QCheckBox::toggled,
            this, &SpatialOverlayPlotPropertiesWidget::onTooltipsEnabledChanged);

    // Selection settings
    connect(ui->selection_mode_combo, &QComboBox::currentIndexChanged,
            this, &SpatialOverlayPlotPropertiesWidget::onSelectionModeChanged);

    connect(ui->clear_selection_button, &QPushButton::clicked,
            this, &SpatialOverlayPlotPropertiesWidget::onClearSelectionClicked);
    
}

void SpatialOverlayPlotPropertiesWidget::refreshDataSourcesList() {
    ui->data_sources_list->clear();

    if (!_data_manager) {
        qDebug() << "SpatialOverlayPlotPropertiesWidget: No data manager available";
        return;
    }

    // Get all PointData objects from DataManager
    auto point_data_keys = _data_manager->getKeys<PointData>();
    qDebug() << "SpatialOverlayPlotPropertiesWidget: Found" << point_data_keys.size() << "PointData keys";

    for (auto const & key: point_data_keys) {
        qDebug() << "SpatialOverlayPlotPropertiesWidget: Adding data source:" << QString::fromStdString(key);
        QListWidgetItem * item = new QListWidgetItem(QString::fromStdString(key), ui->data_sources_list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        ui->data_sources_list->addItem(item);
    }
}

QStringList SpatialOverlayPlotPropertiesWidget::getSelectedDataSources() const {
    QStringList selected_keys;

    for (int i = 0; i < ui->data_sources_list->count(); ++i) {
        QListWidgetItem * item = ui->data_sources_list->item(i);
        if (item->checkState() == Qt::Checked) {
            selected_keys.append(item->text());
        }
    }

    return selected_keys;
}

void SpatialOverlayPlotPropertiesWidget::setSelectedDataSources(QStringList const & selected_keys) {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: setSelectedDataSources called with keys:" << selected_keys;
    qDebug() << "SpatialOverlayPlotPropertiesWidget: List widget has" << ui->data_sources_list->count() << "items";
    
    // Block signals to prevent recursion when setting checkbox states
    ui->data_sources_list->blockSignals(true);
    
    for (int i = 0; i < ui->data_sources_list->count(); ++i) {
        QListWidgetItem * item = ui->data_sources_list->item(i);
        bool should_be_checked = selected_keys.contains(item->text());
        qDebug() << "SpatialOverlayPlotPropertiesWidget: Setting item" << item->text() << "to checked:" << should_be_checked;
        
        if (should_be_checked) {
            item->setCheckState(Qt::Checked);
        } else {
            item->setCheckState(Qt::Unchecked);
        }
    }
    
    // Re-enable signals
    ui->data_sources_list->blockSignals(false);
}

void SpatialOverlayPlotPropertiesWidget::updatePlotWidget() {
    if (!_spatial_plot_widget) {
        qDebug() << "SpatialOverlayPlotPropertiesWidget: No spatial plot widget available";
        return;
    }

    // Update selected data sources
    QStringList selected_keys = getSelectedDataSources();
    qDebug() << "SpatialOverlayPlotPropertiesWidget: Updating plot with selected keys:" << selected_keys;
    _spatial_plot_widget->setPointDataKeys(selected_keys);

    // Update visualization settings
    // Note: We'll need to add public interfaces to SpatialOverlayPlotWidget for these
    // For now, the basic data source update is working
}

void SpatialOverlayPlotPropertiesWidget::onSelectionModeChanged(int index) {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: onSelectionModeChanged called with index:" << index;
    
    if (index < 0 || index >= ui->selection_mode_combo->count()) {
        qDebug() << "SpatialOverlayPlotPropertiesWidget: Invalid index, ignoring";
        return;
    }
    
    // Get the SelectionMode from the combo box data
    SelectionMode mode = SelectionMode::None;

    if (index == 1) {
        mode = SelectionMode::PointSelection;
    } else if (index == 2) {
        mode = SelectionMode::PolygonSelection;
    }

    qDebug() << "SpatialOverlayPlotPropertiesWidget: Setting selection mode to:" << static_cast<int>(mode);
    
    
    if (_spatial_plot_widget) {
        _spatial_plot_widget->setSelectionMode(mode);

        // Update instruction text first
        updateSelectionInstructions();
        
        // Update clear selection button enabled state
        ui->clear_selection_button->setEnabled(mode != SelectionMode::None);

    }
}

void SpatialOverlayPlotPropertiesWidget::onClearSelectionClicked() {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: onClearSelectionClicked called";
    
    if (_spatial_plot_widget && _spatial_plot_widget->getOpenGLWidget()) {
        _spatial_plot_widget->getOpenGLWidget()->clearSelection();
        qDebug() << "SpatialOverlayPlotPropertiesWidget: Selection cleared";
    }
}

void SpatialOverlayPlotPropertiesWidget::updateSelectionInstructions() {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: updateSelectionInstructions called";
    
    if (!ui->selection_instructions_label || !ui->selection_mode_combo) {
        qDebug() << "SpatialOverlayPlotPropertiesWidget: Missing UI elements, returning";
        return;
    }
    
    int current_index = ui->selection_mode_combo->currentIndex();
    qDebug() << "SpatialOverlayPlotPropertiesWidget: Current combo index:" << current_index;
    
    if (current_index < 0 || current_index >= ui->selection_mode_combo->count()) {
        qDebug() << "SpatialOverlayPlotPropertiesWidget: Invalid combo index, returning";
        return;
    }
    
    SelectionMode mode = SelectionMode::None;   

    if (current_index == 1) {
        mode = SelectionMode::PointSelection;
    } else if (current_index == 2) {
        mode = SelectionMode::PolygonSelection;
    }

    qDebug() << "SpatialOverlayPlotPropertiesWidget: Current selection mode:" << static_cast<int>(mode);
    
    QString instructions;
    
    switch (mode) {
        case SelectionMode::None:
            instructions = "📍 Selection Disabled\n"
                          "No point selection available in this mode.";
            break;
            
        case SelectionMode::PointSelection:
            instructions = "🖱️ Point Selection Mode\n"
                          "• Hold Ctrl + Left Click on individual points to select/deselect\n"
                          "• Selected points appear in black\n"
                          "• Double-click points to jump to that frame";
            break;
            
        case SelectionMode::PolygonSelection:
            instructions = "📐 Polygon Selection Mode\n"
                          "• Left Click to add vertices to polygon\n"
                          "• Right Click to complete polygon and select enclosed points\n"
                          "• Press Escape to cancel current polygon\n"
                          "• Red dots show vertices, blue lines show edges";
            break;
    }
    
    qDebug() << "SpatialOverlayPlotPropertiesWidget: Setting instructions:" << instructions;
    ui->selection_instructions_label->setText(instructions);
}