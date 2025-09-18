#include "SpatialOverlayPlotPropertiesWidget.hpp"
#include "ui_SpatialOverlayPlotPropertiesWidget.h"

#include "SpatialOverlayOpenGLWidget.hpp"
#include "SpatialOverlayPlotWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "Feature_Table_Widget/Feature_Table_Widget.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include <memory>

SpatialOverlayPlotPropertiesWidget::SpatialOverlayPlotPropertiesWidget(QWidget * parent)
    : AbstractPlotPropertiesWidget(parent),
      ui(new Ui::SpatialOverlayPlotPropertiesWidget),
      _spatial_plot_widget(nullptr),
      _start_frame(0),
      _end_frame(999999),
      _total_frame_count(0),
      _line_width(2.0),
      _updating_selection_status(false) {
    ui->setupUi(this);

    // Prevent infinite layout loops by setting fixed sizes on dynamic labels
    if (ui->active_dataset_label) {
        ui->active_dataset_label->setMinimumHeight(20);
        ui->active_dataset_label->setMaximumHeight(40);
    }
    if (ui->selection_count_label) {
        ui->selection_count_label->setMinimumHeight(20);
        ui->selection_count_label->setMaximumHeight(40);
    }
    // CRITICAL FIX: Prevent infinite layout loop from word-wrapped instructions label
    if (ui->selection_instructions_label) {
        ui->selection_instructions_label->setMinimumHeight(120);
        ui->selection_instructions_label->setMaximumHeight(150);
        ui->selection_instructions_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    setupFeatureTable();
    setupConnections();
}

SpatialOverlayPlotPropertiesWidget::~SpatialOverlayPlotPropertiesWidget() {
    delete ui;
}

void SpatialOverlayPlotPropertiesWidget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = std::move(data_manager);
    if (_data_manager) {
        ui->feature_table_widget->setDataManager(_data_manager);
        updateAvailableDataSources();
    }
}

void SpatialOverlayPlotPropertiesWidget::setPlotWidget(AbstractPlotWidget * plot_widget) {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: setPlotWidget called with plot widget:" << (plot_widget != nullptr);

    _spatial_plot_widget = qobject_cast<SpatialOverlayPlotWidget *>(plot_widget);
    qDebug() << "SpatialOverlayPlotPropertiesWidget: Cast to SpatialOverlayPlotWidget:" << (_spatial_plot_widget != nullptr);

    if (_spatial_plot_widget) {
        qDebug() << "SpatialOverlayPlotPropertiesWidget: Updating available data sources and UI";

        connect(_spatial_plot_widget, &SpatialOverlayPlotWidget::selectionChanged,
                this, [this](size_t selectedCount) {
                    qDebug() << "SpatialOverlayPlotPropertiesWidget: Selection changed, count:" << selectedCount;
                    updateSelectionStatus();
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

        // Update available data sources if data manager is available. Might be null during initial setup.
        if (_data_manager) {
            qDebug() << "SpatialOverlayPlotPropertiesWidget: DataManager available, updating data sources";
            updateAvailableDataSources();
        } else {
            qDebug() << "SpatialOverlayPlotPropertiesWidget: No DataManager available";
        }

        // Update UI from plot
        updateFromPlot();

        // Initialize selection instructions
        updateSelectionInstructions();

        // Initialize selection mode combo box with available modes
        updateSelectionModeComboBox();
    }
}

void SpatialOverlayPlotPropertiesWidget::updateFromPlot() {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: updateFromPlot called";
    if (_spatial_plot_widget) {
        // Update UI from current plot state
        QStringList current_keys = _spatial_plot_widget->getPointDataKeys();
        qDebug() << "SpatialOverlayPlotPropertiesWidget: updateFromPlot - current keys from plot:" << current_keys;
        setSelectedDataSources(current_keys);

        // Update point size from current widget state
        if (_spatial_plot_widget->getOpenGLWidget()) {

            float current_point_size = _spatial_plot_widget->getOpenGLWidget()->getPointSize();
            ui->point_size_spinbox->blockSignals(true);
            ui->point_size_spinbox->setValue(static_cast<double>(current_point_size));
            ui->point_size_spinbox->blockSignals(false);

            float current_line_width = _spatial_plot_widget->getOpenGLWidget()->getLineWidth();
            ui->line_width_spinbox->blockSignals(true);
            ui->line_width_spinbox->setValue(static_cast<double>(current_line_width));
            ui->line_width_spinbox->blockSignals(false);

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

        // Update selection status display
        updateSelectionStatus();
    } else {
        qDebug() << "SpatialOverlayPlotPropertiesWidget: updateFromPlot - no spatial plot widget available";
    }
}

void SpatialOverlayPlotPropertiesWidget::applyToPlot() {
    updatePlotWidget();
}

void SpatialOverlayPlotPropertiesWidget::setupFeatureTable() {
    if (ui->feature_table_widget) {

        ui->feature_table_widget->setColumns({"Feature", "Type", "Enabled"});
        ui->feature_table_widget->setTypeFilter({DM_DataType::Points, DM_DataType::Mask, DM_DataType::Line});

        // Connect signals from the feature table
        connect(ui->feature_table_widget, &Feature_Table_Widget::featureSelected,
                this, &SpatialOverlayPlotPropertiesWidget::onFeatureSelected);
        connect(ui->feature_table_widget, &Feature_Table_Widget::addFeature,
                this, &SpatialOverlayPlotPropertiesWidget::onFeatureAdded);
        connect(ui->feature_table_widget, &Feature_Table_Widget::removeFeature,
                this, &SpatialOverlayPlotPropertiesWidget::onFeatureRemoved);
    }
}

void SpatialOverlayPlotPropertiesWidget::updateAvailableDataSources() {
    if (ui->feature_table_widget) {
        ui->feature_table_widget->populateTable();
    }
}

void SpatialOverlayPlotPropertiesWidget::onFeatureSelected(QString const & feature) {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: onFeatureSelected called for feature:" << feature;
    // Feature selection is handled by the feature table widget itself
}

void SpatialOverlayPlotPropertiesWidget::onFeatureAdded(QString const & feature) {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: onFeatureAdded called for feature:" << feature;
    if (!_selected_features.contains(feature)) {
        _selected_features.append(feature);
    }
    updatePlotWidget();
    updateSelectionModeComboBox();
}

void SpatialOverlayPlotPropertiesWidget::onFeatureRemoved(QString const & feature) {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: onFeatureRemoved called for feature:" << feature;
    _selected_features.removeAll(feature);
    updatePlotWidget();
    updateSelectionModeComboBox();
}

void SpatialOverlayPlotPropertiesWidget::onPointSizeChanged(double value) {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: onPointSizeChanged called with value:" << value;
    if (_spatial_plot_widget && _spatial_plot_widget->getOpenGLWidget()) {
        _spatial_plot_widget->getOpenGLWidget()->setPointSize(static_cast<float>(value));
    }
}

void SpatialOverlayPlotPropertiesWidget::onLineWidthChanged(double value) {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: onLineWidthChanged called with value:" << value;
    _line_width = value;
    if (_spatial_plot_widget && _spatial_plot_widget->getOpenGLWidget()) {
        _spatial_plot_widget->getOpenGLWidget()->setLineWidth(static_cast<float>(value));
    }
}


void SpatialOverlayPlotPropertiesWidget::onResetViewClicked() {
    if (_spatial_plot_widget && _spatial_plot_widget->getOpenGLWidget()) {
        // Reset view to fit all data (zoom and pan to defaults)
        _spatial_plot_widget->getOpenGLWidget()->resetView();
    }
}

void SpatialOverlayPlotPropertiesWidget::onTooltipsEnabledChanged(bool enabled) {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: onTooltipsEnabledChanged called with enabled:" << enabled;
    if (_spatial_plot_widget && _spatial_plot_widget->getOpenGLWidget()) {
        _spatial_plot_widget->getOpenGLWidget()->setTooltipsEnabled(enabled);
    }
}


void SpatialOverlayPlotPropertiesWidget::setupConnections() {

    // Visualization settings
    connect(ui->point_size_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SpatialOverlayPlotPropertiesWidget::onPointSizeChanged);

    connect(ui->line_width_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SpatialOverlayPlotPropertiesWidget::onLineWidthChanged);


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

    // Time range filtering
    connect(ui->start_frame_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SpatialOverlayPlotPropertiesWidget::onStartFrameChanged);

    connect(ui->end_frame_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SpatialOverlayPlotPropertiesWidget::onEndFrameChanged);

    connect(ui->update_time_range_button, &QPushButton::clicked,
            this, &SpatialOverlayPlotPropertiesWidget::onUpdateTimeRangeClicked);
}


QStringList SpatialOverlayPlotPropertiesWidget::getSelectedDataSources() const {
    return _selected_features;
}

void SpatialOverlayPlotPropertiesWidget::setSelectedDataSources(QStringList const & selected_keys) {
    qDebug() << "SpatialOverlayPlotPropertiesWidget: setSelectedDataSources called with keys:" << selected_keys;

    _selected_features = selected_keys;

    // TODO: Update the feature table UI to reflect the selected state
    // This would require the Feature_Table_Widget to provide methods to
    // programmatically set checkbox states
}

void SpatialOverlayPlotPropertiesWidget::updatePlotWidget() {
    if (!_spatial_plot_widget) {
        qDebug() << "SpatialOverlayPlotPropertiesWidget: No spatial plot widget available";
        return;
    }

    // Update selected data sources
    QStringList selected_keys = getSelectedDataSources();
    QStringList point_data_keys;
    QStringList mask_data_keys;
    QStringList line_data_keys;

    if (!_data_manager) {
        qDebug() << "SpatialOverlayPlotPropertiesWidget: No data manager available, cannot update plot widget";
        return;
    }

    for (auto const & key: selected_keys) {
        if (_data_manager->getType(key.toStdString()) == DM_DataType::Points) {
            point_data_keys.append(key);
        } else if (_data_manager->getType(key.toStdString()) == DM_DataType::Mask) {
            mask_data_keys.append(key);
        } else if (_data_manager->getType(key.toStdString()) == DM_DataType::Line) {
            line_data_keys.append(key);
        }
    }

    // Always update, even with empty lists, to allow clearing datasets properly
    // Use single aggregated setter to avoid multiple updateVisualization() calls
    _spatial_plot_widget->setDataKeys(point_data_keys, mask_data_keys, line_data_keys);

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
    } else if (index == 3) {
        mode = SelectionMode::LineIntersection;
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

        // Update selection status display
        updateSelectionStatus();
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
    } else if (current_index == 3) {
        mode = SelectionMode::LineIntersection;
    }

    qDebug() << "SpatialOverlayPlotPropertiesWidget: Current selection mode:" << static_cast<int>(mode);

    QString instructions;

    switch (mode) {
        case SelectionMode::None:
            instructions = "📍 Selection Disabled\n"
                           "No point selection available in this mode.";
            break;

        case SelectionMode::PointSelection:
            instructions = "🖱️ Point & Mask Selection Mode\n"
                           "• Ctrl + Left Click: add to selection (no toggle)\n"
                           "• Shift + Left Click: remove from selection\n"
                           "  - Points: removes the clicked point if selected\n"
                           "  - Masks: removes all intersecting masks between current selection and clicked area\n"
                           "• Selected points appear in black; selected masks have enhanced visibility\n"
                           "• Use 'Clear Selection' to remove all selections\n"
                           "• Double-click points to jump to that frame";
            break;

        case SelectionMode::PolygonSelection:
            instructions = "📐 Polygon Selection Mode\n"
                           "• Left Click to add vertices to polygon\n"
                           "• Right Click to complete polygon and select enclosed points\n"
                           "• Press Escape to cancel current polygon\n"
                           "• Red dots show vertices, blue lines show edges";
            break;

        case SelectionMode::LineIntersection:
            instructions = "📏 Line Intersection Selection Mode\n"
                           "• Click and hold to start drawing a line\n"
                           "• Drag to extend the line from start point\n"
                           "• Release to find all lines that intersect with your drawn line\n"
                           "• Only works with Line data - no effect on Points or Masks\n"
                           "• Selected lines will be highlighted";
            break;
    }

    qDebug() << "SpatialOverlayPlotPropertiesWidget: Setting instructions:" << instructions;
    ui->selection_instructions_label->setText(instructions);
}

void SpatialOverlayPlotPropertiesWidget::updateSelectionStatus() {
    qDebug() << "SpatialOverlayPlotPropertiesWidget::updateSelectionStatus() called";

    // Guard against infinite recursion
    if (_updating_selection_status) {
        qDebug() << "SpatialOverlayPlotPropertiesWidget::updateSelectionStatus() - Already updating, skipping to prevent infinite loop";
        return;
    }
    _updating_selection_status = true;

    if (!ui->active_dataset_label || !ui->selection_count_label) {
        qDebug() << "SpatialOverlayPlotPropertiesWidget::updateSelectionStatus() - Missing UI labels";
        _updating_selection_status = false;
        return;
    }

    // Update active dataset information
    QString activeDataset = "None";
    if (_spatial_plot_widget) {
        // Get the current active datasets from the plot widget
        QStringList pointKeys = _spatial_plot_widget->getPointDataKeys();
        QStringList maskKeys = _spatial_plot_widget->getMaskDataKeys();
        QStringList lineKeys = _spatial_plot_widget->getLineDataKeys();

        QStringList allKeys = pointKeys + maskKeys + lineKeys;
        if (!allKeys.isEmpty()) {
            if (allKeys.size() == 1) {
                activeDataset = allKeys.first();
            } else {
                activeDataset = QString("Multiple (%1)").arg(allKeys.size());
            }
        }
    }

    ui->active_dataset_label->setText(QString("Active Dataset: %1").arg(activeDataset));

    // Update selection counts
    size_t pointCount = 0;
    size_t maskCount = 0;
    size_t lineCount = 0;

    if (_spatial_plot_widget && _spatial_plot_widget->getOpenGLWidget()) {
        pointCount = _spatial_plot_widget->getOpenGLWidget()->getTotalSelectedPoints();
        maskCount = _spatial_plot_widget->getOpenGLWidget()->getTotalSelectedMasks();
        lineCount = _spatial_plot_widget->getOpenGLWidget()->getTotalSelectedLines();
    }

    QString selectionText = QString("Selected: %1 points, %2 masks, %3 lines")
                                    .arg(pointCount)
                                    .arg(maskCount)
                                    .arg(lineCount);

    ui->selection_count_label->setText(selectionText);

    qDebug() << "SpatialOverlayPlotPropertiesWidget: Updated selection status - Dataset:" << activeDataset
             << "Selected:" << pointCount << "points," << maskCount << "masks," << lineCount << "lines";
    qDebug() << "SpatialOverlayPlotPropertiesWidget: Set label text to:" << selectionText;

    _updating_selection_status = false;
}

void SpatialOverlayPlotPropertiesWidget::onUpdateTimeRangeClicked() {
    qDebug() << "SpatialOverlayPlotPropertiesWidget::onUpdateTimeRangeClicked() called";

    _start_frame = ui->start_frame_spinbox->value();
    _end_frame = ui->end_frame_spinbox->value();

    qDebug() << "Time range updated: start=" << _start_frame << ", end=" << _end_frame;

    updateTimeRangeFilter();
}

void SpatialOverlayPlotPropertiesWidget::onStartFrameChanged(int value) {
    // Ensure start frame doesn't exceed end frame
    if (value > ui->end_frame_spinbox->value()) {
        ui->end_frame_spinbox->setValue(value);
    }
}

void SpatialOverlayPlotPropertiesWidget::onEndFrameChanged(int value) {
    // Ensure end frame isn't less than start frame
    if (value < ui->start_frame_spinbox->value()) {
        ui->start_frame_spinbox->setValue(value);
    }
}

void SpatialOverlayPlotPropertiesWidget::setupTimeRangeControls() {
    qDebug() << "SpatialOverlayPlotPropertiesWidget::setupTimeRangeControls() called";

    if (!_data_manager) {
        qDebug() << "SpatialOverlayPlotPropertiesWidget::setupTimeRangeControls() - No data manager available";
        return;
    }
    // Get total frame count from data manager
    _total_frame_count = 0;
    {
        try {
            auto timeFrame = _data_manager->getTime(TimeKey("time"));
            if (timeFrame) {
                _total_frame_count = static_cast<int>(timeFrame->getTotalFrameCount());
                qDebug() << "Total frame count from data manager:" << _total_frame_count;
            }
        } catch (std::exception const & e) {
            qDebug() << "Error getting time frame:" << e.what();
        }
    }

    // Update spinbox ranges
    if (_total_frame_count > 0) {
        ui->start_frame_spinbox->setMaximum(_total_frame_count - 1);
        ui->end_frame_spinbox->setMaximum(_total_frame_count - 1);
        ui->end_frame_spinbox->setValue(_total_frame_count - 1);
        _end_frame = _total_frame_count - 1;
    }

    qDebug() << "Time range controls setup complete - max frame:" << (_total_frame_count - 1);
}

void SpatialOverlayPlotPropertiesWidget::updateTimeRangeFilter() {
    qDebug() << "SpatialOverlayPlotPropertiesWidget::updateTimeRangeFilter() called with range:"
             << _start_frame << "to" << _end_frame;

    if (!_spatial_plot_widget) {
        qDebug() << "No spatial plot widget available for time range filtering";
        return;
    }

    // Apply time range filter to the OpenGL widget
    auto openglWidget = _spatial_plot_widget->getOpenGLWidget();

    if (openglWidget) {
        qDebug() << "Applying time range filter to OpenGL widget";
        openglWidget->applyTimeRangeFilter(_start_frame, _end_frame);
    }
}

QList<SelectionMode> SpatialOverlayPlotPropertiesWidget::getAvailableSelectionModes() const {
    QList<SelectionMode> availableModes;

    // Always include None as the default option
    availableModes.append(SelectionMode::None);

    if (!_spatial_plot_widget) {
        return availableModes;
    }

    // Get currently enabled data types
    QStringList pointKeys = _spatial_plot_widget->getPointDataKeys();
    QStringList maskKeys = _spatial_plot_widget->getMaskDataKeys();
    QStringList lineKeys = _spatial_plot_widget->getLineDataKeys();

    bool hasPoints = !pointKeys.isEmpty();
    bool hasMasks = !maskKeys.isEmpty();
    bool hasLines = !lineKeys.isEmpty();

    // Points allow for point selection and polygon selection
    if (hasPoints) {
        availableModes.append(SelectionMode::PointSelection);
        availableModes.append(SelectionMode::PolygonSelection);
    }

    // Masks allow for point selection
    if (hasMasks) {
        if (!availableModes.contains(SelectionMode::PointSelection)) {
            availableModes.append(SelectionMode::PointSelection);
        }
    }

    // Lines allow for line selection
    if (hasLines) {
        availableModes.append(SelectionMode::LineIntersection);
    }

    return availableModes;
}

void SpatialOverlayPlotPropertiesWidget::updateSelectionModeComboBox() {
    if (!ui->selection_mode_combo) {
        return;
    }

    // Get currently available selection modes
    QList<SelectionMode> availableModes = getAvailableSelectionModes();

    // Store current selection
    SelectionMode currentMode = SelectionMode::None;
    int currentIndex = ui->selection_mode_combo->currentIndex();
    if (currentIndex >= 0 && currentIndex < ui->selection_mode_combo->count()) {
        currentMode = static_cast<SelectionMode>(ui->selection_mode_combo->itemData(currentIndex).toInt());
    }

    // Block signals to prevent triggering change events during update
    ui->selection_mode_combo->blockSignals(true);

    // Clear existing items
    ui->selection_mode_combo->clear();

    // Add available modes
    for (SelectionMode mode: availableModes) {
        QString text;
        int dataValue = static_cast<int>(mode);

        switch (mode) {
            case SelectionMode::None:
                text = "None";
                break;
            case SelectionMode::PointSelection:
                text = "Point & Mask Selection (Ctrl+Click)";
                break;
            case SelectionMode::PolygonSelection:
                text = "Polygon Selection";
                break;
            case SelectionMode::LineIntersection:
                text = "Line Intersection Selection";
                break;
        }

        ui->selection_mode_combo->addItem(text, dataValue);
    }

    // Restore previous selection if it's still available, otherwise default to None
    int newIndex = 0;// Default to None
    for (int i = 0; i < ui->selection_mode_combo->count(); ++i) {
        if (static_cast<SelectionMode>(ui->selection_mode_combo->itemData(i).toInt()) == currentMode) {
            newIndex = i;
            break;
        }
    }

    ui->selection_mode_combo->setCurrentIndex(newIndex);

    // Re-enable signals
    ui->selection_mode_combo->blockSignals(false);

    // Update clear selection button state
    SelectionMode selectedMode = static_cast<SelectionMode>(ui->selection_mode_combo->itemData(newIndex).toInt());
    ui->clear_selection_button->setEnabled(selectedMode != SelectionMode::None);

    // Update instructions
    updateSelectionInstructions();

    qDebug() << "Updated selection mode combo box with" << availableModes.size() << "available modes";
}