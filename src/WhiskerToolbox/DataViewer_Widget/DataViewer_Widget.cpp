#include "DataViewer_Widget.hpp"
#include "ui_DataViewer_Widget.h"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/utils/statistics.hpp"
#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include "DataViewer/AnalogTimeSeries/AnalogTimeSeriesDisplayOptions.hpp"
#include "DataViewer/AnalogTimeSeries/MVP_AnalogTimeSeries.hpp"
#include "DataViewer/DigitalEvent/DigitalEventSeriesDisplayOptions.hpp"
#include "DataViewer/DigitalEvent/MVP_DigitalEvent.hpp"
#include "DataViewer/DigitalInterval/DigitalIntervalSeriesDisplayOptions.hpp"
#include "DataViewer/DigitalInterval/MVP_DigitalInterval.hpp"
#include "Feature_Tree_Model.hpp"
#include "Feature_Tree_Widget/Feature_Tree_Widget.hpp"
#include "OpenGLWidget.hpp"
#include "SVGExporter.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"

#include "AnalogTimeSeries/AnalogViewer_Widget.hpp"
#include "DigitalEvent/EventViewer_Widget.hpp"
#include "DigitalInterval/IntervalViewer_Widget.hpp"

#include <QMetaObject>
#include <QPointer>
#include <QTableWidget>
#include <QWheelEvent>
#include <QMenu>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>

DataViewer_Widget::DataViewer_Widget(std::shared_ptr<DataManager> data_manager,
                                     TimeScrollBar * time_scrollbar,
                                     MainWindow * main_window,
                                     QWidget * parent)
    : QWidget(parent),
      _data_manager{std::move(data_manager)},
      _time_scrollbar{time_scrollbar},
      _main_window{main_window},
      ui(new Ui::DataViewer_Widget) {

    ui->setupUi(this);

    // Initialize plotting manager with default viewport
    _plotting_manager = std::make_unique<PlottingManager>();

    // Provide PlottingManager reference to OpenGL widget
    ui->openGLWidget->setPlottingManager(_plotting_manager.get());

    // Initialize feature tree model
    _feature_tree_model = std::make_unique<Feature_Tree_Model>(this);
    _feature_tree_model->setDataManager(_data_manager);

    // Set up observer to automatically clean up data when it's deleted from DataManager
    // Queue the cleanup to the Qt event loop to avoid running during mid-update mutations
    _data_manager->addObserver([this]() {
        QPointer<DataViewer_Widget> self = this;
        QMetaObject::invokeMethod(self, [self]() {
            if (!self) return;
            self->cleanupDeletedData(); }, Qt::QueuedConnection);
    });

    // Configure Feature_Tree_Widget
    ui->feature_tree_widget->setTypeFilters({DM_DataType::Analog, DM_DataType::DigitalEvent, DM_DataType::DigitalInterval});
    ui->feature_tree_widget->setDataManager(_data_manager);

    // Connect Feature_Tree_Widget signals using the new interface
    connect(ui->feature_tree_widget, &Feature_Tree_Widget::featureSelected, this, [this](std::string const & feature) {
        _handleFeatureSelected(QString::fromStdString(feature));
    });

    connect(ui->feature_tree_widget, &Feature_Tree_Widget::addFeature, this, [this](std::string const & feature) {
        std::cout << "Adding single feature: " << feature << std::endl;
        _addFeatureToModel(QString::fromStdString(feature), true);
    });

    // Install context menu handling on the embedded tree widget
    ui->feature_tree_widget->treeWidget()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->feature_tree_widget->treeWidget(), &QTreeWidget::customContextMenuRequested, this, [this](QPoint const & pos) {
        auto * tw = ui->feature_tree_widget->treeWidget();
        QTreeWidgetItem * item = tw->itemAt(pos);
        if (!item) return;
        std::string const key = item->text(0).toStdString();
        // Group items have children and are under Analog data type parent
        bool const hasChildren = item->childCount() > 0;
        if (hasChildren) {
            // Determine if parent is Analog data type or children are analog keys
            bool isAnalogGroup = false;
            if (auto * parent = item->parent()) {
                if (parent->text(0) == QString::fromStdString(convert_data_type_to_string(DM_DataType::Analog))) {
                    isAnalogGroup = true;
                }
            }
            if (isAnalogGroup) {
                QPoint const global_pos = tw->viewport()->mapToGlobal(pos);
                _showGroupContextMenu(key, global_pos);
            }
        }
    });

    connect(ui->feature_tree_widget, &Feature_Tree_Widget::removeFeature, this, [this](std::string const & feature) {
        std::cout << "Removing single feature: " << feature << std::endl;
        _addFeatureToModel(QString::fromStdString(feature), false);
    });

    connect(ui->feature_tree_widget, &Feature_Tree_Widget::addFeatures, this, [this](std::vector<std::string> const & features) {
        std::cout << "Adding " << features.size() << " features as group" << std::endl;

        // Mark batch add to suppress per-series auto-arrange
        _is_batch_add = true;
        // Process all features in the group without triggering individual canvas updates
        for (auto const & key: features) {
            _plotSelectedFeatureWithoutUpdate(key);
        }
        _is_batch_add = false;

        // Auto-arrange and auto-fill once after batch
        if (!features.empty()) {
            std::cout << "Auto-arranging and filling canvas for group toggle" << std::endl;
            autoArrangeVerticalSpacing();// includes auto-fill functionality
        }

        // Trigger a single canvas update at the end
        if (!features.empty()) {
            std::cout << "Triggering single canvas update for group toggle" << std::endl;
            ui->openGLWidget->updateCanvas();
        }
    });

    connect(ui->feature_tree_widget, &Feature_Tree_Widget::removeFeatures, this, [this](std::vector<std::string> const & features) {
        std::cout << "Removing " << features.size() << " features as group" << std::endl;

        _is_batch_add = true;
        // Process all features in the group without triggering individual canvas updates
        for (auto const & key: features) {
            _removeSelectedFeatureWithoutUpdate(key);
        }
        _is_batch_add = false;

        // Auto-arrange and auto-fill once after batch
        if (!features.empty()) {
            std::cout << "Auto-arranging and filling canvas for group toggle" << std::endl;
            autoArrangeVerticalSpacing();// includes auto-fill functionality
        }

        // Trigger a single canvas update at the end
        if (!features.empty()) {
            std::cout << "Triggering single canvas update for group toggle" << std::endl;
            ui->openGLWidget->updateCanvas();
        }
    });

    // Connect color change signals from the model
    connect(_feature_tree_model.get(), &Feature_Tree_Model::featureColorChanged, this, &DataViewer_Widget::_handleColorChanged);

    // Connect color change signals from the tree widget to the model
    connect(ui->feature_tree_widget, &Feature_Tree_Widget::colorChangeFeatures, this, [this](std::vector<std::string> const & features, std::string const & hex_color) {
        for (auto const & feature: features) {
            _feature_tree_model->setFeatureColor(feature, hex_color);
        }
    });

    connect(ui->x_axis_samples, QOverload<int>::of(&QSpinBox::valueChanged), this, &DataViewer_Widget::_handleXAxisSamplesChanged);

    connect(ui->global_zoom, &QDoubleSpinBox::valueChanged, this, &DataViewer_Widget::_updateGlobalScale);

    connect(ui->theme_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DataViewer_Widget::_handleThemeChanged);

    connect(time_scrollbar, &TimeScrollBar::timeChanged, this, &DataViewer_Widget::_updatePlot);

    //We should alwasy get the master clock because we plot
    // Check for master clock
    auto time_keys = _data_manager->getTimeFrameKeys();
    // if timekeys doesn't have master, we should throw an error
    if (std::find(time_keys.begin(), time_keys.end(), TimeKey("master")) == time_keys.end()) {
        std::cout << "No master clock found in DataManager" << std::endl;
        _time_frame = _data_manager->getTime(TimeKey("time"));
    } else {
        _time_frame = _data_manager->getTime(TimeKey("master"));
    }

    std::cout << "Setting GL limit to " << _time_frame->getTotalFrameCount() << std::endl;
    ui->openGLWidget->setXLimit(_time_frame->getTotalFrameCount());

    // Set the master time frame for proper coordinate conversion
    ui->openGLWidget->setMasterTimeFrame(_time_frame);

    // Set spinbox maximum to the actual data range (not the hardcoded UI limit)
    int const data_range = static_cast<int>(_time_frame->getTotalFrameCount());
    std::cout << "Setting x_axis_samples maximum to " << data_range << std::endl;
    ui->x_axis_samples->setMaximum(data_range);

    // Setup stacked widget with data-type specific viewers
    auto analog_widget = new AnalogViewer_Widget(_data_manager, ui->openGLWidget);
    auto interval_widget = new IntervalViewer_Widget(_data_manager, ui->openGLWidget);
    auto event_widget = new EventViewer_Widget(_data_manager, ui->openGLWidget);

    ui->stackedWidget->addWidget(analog_widget);
    ui->stackedWidget->addWidget(interval_widget);
    ui->stackedWidget->addWidget(event_widget);

    // Connect color change signals from sub-widgets
    connect(analog_widget, &AnalogViewer_Widget::colorChanged,
            this, &DataViewer_Widget::_handleColorChanged);
    connect(interval_widget, &IntervalViewer_Widget::colorChanged,
            this, &DataViewer_Widget::_handleColorChanged);
    connect(event_widget, &EventViewer_Widget::colorChanged,
            this, &DataViewer_Widget::_handleColorChanged);

    // Connect mouse hover signal from OpenGL widget
    connect(ui->openGLWidget, &OpenGLWidget::mouseHover,
            this, &DataViewer_Widget::_updateCoordinateDisplay);

    // Grid line connections
    connect(ui->grid_lines_enabled, &QCheckBox::toggled, this, &DataViewer_Widget::_handleGridLinesToggled);
    connect(ui->grid_spacing, QOverload<int>::of(&QSpinBox::valueChanged), this, &DataViewer_Widget::_handleGridSpacingChanged);

    // Vertical spacing connection
    connect(ui->vertical_spacing, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &DataViewer_Widget::_handleVerticalSpacingChanged);

    // Auto-arrange button connection
    connect(ui->auto_arrange_button, &QPushButton::clicked, this, &DataViewer_Widget::autoArrangeVerticalSpacing);

    // Initialize grid line UI to match OpenGLWidget defaults
    ui->grid_lines_enabled->setChecked(ui->openGLWidget->getGridLinesEnabled());
    ui->grid_spacing->setValue(ui->openGLWidget->getGridSpacing());

    // Initialize vertical spacing UI to match OpenGLWidget defaults
    ui->vertical_spacing->setValue(static_cast<double>(ui->openGLWidget->getVerticalSpacing()));

    // Configure splitter behavior
    ui->main_splitter->setStretchFactor(0, 0);  // Properties panel doesn't stretch
    ui->main_splitter->setStretchFactor(1, 1);  // Plot area stretches
    
    // Set initial sizes: properties panel gets enough space for controls, plot area gets the rest
    ui->main_splitter->setSizes({320, 1000});
    
    // Prevent plot area from collapsing, but allow properties panel to collapse
    ui->main_splitter->setCollapsible(0, true);  // Properties panel can collapse
    ui->main_splitter->setCollapsible(1, false); // Plot area cannot collapse
    
    // Connect hide button (on properties panel)
    connect(ui->hide_properties_button, &QPushButton::clicked, this, [this]() {
        _hidePropertiesPanel();
    });
    
    // Connect show button (on plot side) - initially hidden
    connect(ui->show_properties_button, &QPushButton::clicked, this, [this]() {
        _showPropertiesPanel();
    });
    
    // Initially hide the show button since properties are visible
    ui->show_properties_button->hide();

    // Connect SVG export button
    connect(ui->export_svg_button, &QPushButton::clicked, this, &DataViewer_Widget::_exportToSVG);

    // Connect scalebar checkbox to enable/disable the length spinbox
    connect(ui->svg_scalebar_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
        ui->scalebar_length_spinbox->setEnabled(checked);
        ui->scalebar_length_label->setEnabled(checked);
    });
}

DataViewer_Widget::~DataViewer_Widget() {
    delete ui;
}

void DataViewer_Widget::openWidget() {
    std::cout << "DataViewer Widget Opened" << std::endl;

    // Tree is already populated by observer pattern in setDataManager()
    // Trigger refresh in case of manual opening
    ui->feature_tree_widget->refreshTree();

    this->show();
    _updateLabels();
}

void DataViewer_Widget::closeEvent(QCloseEvent * event) {
    static_cast<void>(event);

    std::cout << "Close event detected" << std::endl;
}

void DataViewer_Widget::resizeEvent(QResizeEvent * event) {
    QWidget::resizeEvent(event);

    // Update plotting manager dimensions when widget is resized
    _updatePlottingManagerDimensions();

    // The OpenGL widget will automatically get its resizeGL called by Qt
    // but we can trigger an additional update if needed
    if (ui->openGLWidget) {
        ui->openGLWidget->update();
    }
}

void DataViewer_Widget::_updatePlot(int time) {
    //std::cout << "Time is " << time;
    time = _data_manager->getTime(TimeKey("time"))->getTimeAtIndex(TimeFrameIndex(time));
    //std::cout << ""
    ui->openGLWidget->updateCanvas(time);

    _updateLabels();
}


void DataViewer_Widget::_addFeatureToModel(QString const & feature, bool enabled) {
    std::cout << "Feature toggle signal received: " << feature.toStdString() << " enabled: " << enabled << std::endl;

    if (enabled) {
        _plotSelectedFeature(feature.toStdString());
    } else {
        _removeSelectedFeature(feature.toStdString());
    }
}

void DataViewer_Widget::_plotSelectedFeature(std::string const & key) {
    std::cout << "Attempting to plot feature: " << key << std::endl;

    if (key.empty()) {
        std::cerr << "Error: empty key in _plotSelectedFeature" << std::endl;
        return;
    }

    if (!_data_manager) {
        std::cerr << "Error: null data manager in _plotSelectedFeature" << std::endl;
        return;
    }

    // Get color from model
    std::string color = _feature_tree_model->getFeatureColor(key);
    std::cout << "Using color: " << color << " for series: " << key << std::endl;

    auto data_type = _data_manager->getType(key);
    std::cout << "Feature type: " << convert_data_type_to_string(data_type) << std::endl;

    // Register with plotting manager for coordinated positioning
    if (_plotting_manager) {
        std::cout << "Registering series with plotting manager: " << key << std::endl;
    }

    if (data_type == DM_DataType::Analog) {

        std::cout << "Adding << " << key << " to PlottingManager and OpenGLWidget" << std::endl;
        auto series = _data_manager->getData<AnalogTimeSeries>(key);
        if (!series) {
            std::cerr << "Error: failed to get AnalogTimeSeries for key: " << key << std::endl;
            return;
        }


        auto time_key = _data_manager->getTimeKey(key);
        std::cout << "Time frame key: " << time_key << std::endl;
        auto time_frame = _data_manager->getTime(time_key);
        if (!time_frame) {
            std::cerr << "Error: failed to get TimeFrame for key: " << key << std::endl;
            return;
        }

        std::cout << "Time frame has " << time_frame->getTotalFrameCount() << " frames" << std::endl;

        // Add to plotting manager first
        _plotting_manager->addAnalogSeries(key, series, time_frame, color);

        ui->openGLWidget->addAnalogTimeSeries(key, series, time_frame, color);
        std::cout << "Successfully added analog series to PlottingManager and OpenGL widget" << std::endl;

    } else if (data_type == DM_DataType::DigitalEvent) {

        std::cout << "Adding << " << key << " to PlottingManager and OpenGLWidget" << std::endl;
        auto series = _data_manager->getData<DigitalEventSeries>(key);
        if (!series) {
            std::cerr << "Error: failed to get DigitalEventSeries for key: " << key << std::endl;
            return;
        }

        auto time_key = _data_manager->getTimeKey(key);
        auto time_frame = _data_manager->getTime(time_key);
        if (!time_frame) {
            std::cerr << "Error: failed to get TimeFrame for key: " << key << std::endl;
            return;
        }

        // Add to plotting manager first
        _plotting_manager->addDigitalEventSeries(key, series, time_frame, color);

        ui->openGLWidget->addDigitalEventSeries(key, series, time_frame, color);

    } else if (data_type == DM_DataType::DigitalInterval) {

        std::cout << "Adding << " << key << " to PlottingManager and OpenGLWidget" << std::endl;
        auto series = _data_manager->getData<DigitalIntervalSeries>(key);
        if (!series) {
            std::cerr << "Error: failed to get DigitalIntervalSeries for key: " << key << std::endl;
            return;
        }

        auto time_key = _data_manager->getTimeKey(key);
        auto time_frame = _data_manager->getTime(time_key);
        if (!time_frame) {
            std::cerr << "Error: failed to get TimeFrame for key: " << key << std::endl;
            return;
        }

        // Add to plotting manager first
        _plotting_manager->addDigitalIntervalSeries(key, series, time_frame, color);

        ui->openGLWidget->addDigitalIntervalSeries(key, series, time_frame, color);

    } else {
        std::cout << "Feature type not supported: " << convert_data_type_to_string(data_type) << std::endl;
        return;
    }

    // Apply coordinated plotting manager allocation after adding to OpenGL widget
    if (_plotting_manager) {
        _applyPlottingManagerAllocation(key);
    }

    // Auto-arrange and auto-fill canvas to make optimal use of space
    // IMPORTANT: Do not auto-arrange when adding DigitalInterval series, since intervals are
    // drawn full-canvas and should not affect analog/event stacking or global zoom.
    if (!_is_batch_add) {
        if (data_type == DM_DataType::DigitalInterval) {
            std::cout << "Skipping auto-arrange after adding DigitalInterval to preserve analog zoom" << std::endl;
        } else {
            std::cout << "Auto-arranging and filling canvas after adding series" << std::endl;
            autoArrangeVerticalSpacing();// This now includes auto-fill functionality
        }
    }

    std::cout << "Series addition and auto-arrangement completed" << std::endl;
    // Trigger canvas update to show the new series
    if (!_is_batch_add) {
        std::cout << "Triggering canvas update" << std::endl;
        ui->openGLWidget->updateCanvas();
    }
    std::cout << "Canvas update completed" << std::endl;
}

void DataViewer_Widget::_removeSelectedFeature(std::string const & key) {
    std::cout << "Attempting to remove feature: " << key << std::endl;

    if (key.empty()) {
        std::cerr << "Error: empty key in _removeSelectedFeature" << std::endl;
        return;
    }

    if (!_data_manager) {
        std::cerr << "Error: null data manager in _removeSelectedFeature" << std::endl;
        return;
    }

    auto data_type = _data_manager->getType(key);

    // Remove from plotting manager first
    if (_plotting_manager) {
        bool removed = false;
        if (data_type == DM_DataType::Analog) {
            removed = _plotting_manager->removeAnalogSeries(key);
        } else if (data_type == DM_DataType::DigitalEvent) {
            removed = _plotting_manager->removeDigitalEventSeries(key);
        } else if (data_type == DM_DataType::DigitalInterval) {
            removed = _plotting_manager->removeDigitalIntervalSeries(key);
        }
        if (removed) {
            std::cout << "Unregistered '" << key << "' from plotting manager" << std::endl;
        }
    }

    if (data_type == DM_DataType::Analog) {
        ui->openGLWidget->removeAnalogTimeSeries(key);
    } else if (data_type == DM_DataType::DigitalEvent) {
        ui->openGLWidget->removeDigitalEventSeries(key);
    } else if (data_type == DM_DataType::DigitalInterval) {
        ui->openGLWidget->removeDigitalIntervalSeries(key);
    } else {
        std::cout << "Feature type not supported for removal: " << convert_data_type_to_string(data_type) << std::endl;
        return;
    }

    // Auto-arrange and auto-fill canvas to rescale remaining elements
    // IMPORTANT: Do not auto-arrange when removing DigitalInterval series; removing an overlay
    // interval should not change analog/event stacking or global zoom.
    if (data_type == DM_DataType::DigitalInterval) {
        std::cout << "Skipping auto-arrange after removing DigitalInterval to preserve analog zoom" << std::endl;
    } else {
        std::cout << "Auto-arranging and filling canvas after removing series" << std::endl;
        autoArrangeVerticalSpacing();// This now includes auto-fill functionality
    }

    std::cout << "Series removal and auto-arrangement completed" << std::endl;
    // Trigger canvas update to reflect the removal
    std::cout << "Triggering canvas update after removal" << std::endl;
    ui->openGLWidget->updateCanvas();
}

void DataViewer_Widget::_handleFeatureSelected(QString const & feature) {
    std::cout << "Feature selected signal received: " << feature.toStdString() << std::endl;

    if (feature.isEmpty()) {
        std::cerr << "Error: empty feature name in _handleFeatureSelected" << std::endl;
        return;
    }

    if (!_data_manager) {
        std::cerr << "Error: null data manager in _handleFeatureSelected" << std::endl;
        return;
    }

    _highlighted_available_feature = feature;

    // Switch stacked widget based on data type
    auto const type = _data_manager->getType(feature.toStdString());
    auto key = feature.toStdString();

    std::cout << "Feature type for selection: " << convert_data_type_to_string(type) << std::endl;

    if (type == DM_DataType::Analog) {
        int const stacked_widget_index = 1;// Analog widget is at index 1 (after empty page)
        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto analog_widget = dynamic_cast<AnalogViewer_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        if (analog_widget) {
            analog_widget->setActiveKey(key);
            std::cout << "Selected Analog Time Series: " << key << std::endl;
        } else {
            std::cerr << "Error: failed to cast to AnalogViewer_Widget" << std::endl;
        }

    } else if (type == DM_DataType::DigitalInterval) {
        int const stacked_widget_index = 2;// Interval widget is at index 2
        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto interval_widget = dynamic_cast<IntervalViewer_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        if (interval_widget) {
            interval_widget->setActiveKey(key);
            std::cout << "Selected Digital Interval Series: " << key << std::endl;
        } else {
            std::cerr << "Error: failed to cast to IntervalViewer_Widget" << std::endl;
        }

    } else if (type == DM_DataType::DigitalEvent) {
        int const stacked_widget_index = 3;// Event widget is at index 3
        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto event_widget = dynamic_cast<EventViewer_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        if (event_widget) {
            event_widget->setActiveKey(key);
            std::cout << "Selected Digital Event Series: " << key << std::endl;
        } else {
            std::cerr << "Error: failed to cast to EventViewer_Widget" << std::endl;
        }

    } else {
        // No specific widget for this type, don't change the current index
        std::cout << "Unsupported feature type for detailed view: " << convert_data_type_to_string(type) << std::endl;
    }
}

void DataViewer_Widget::_handleXAxisSamplesChanged(int value) {
    // Use setRangeWidth for spinbox changes (absolute value)
    std::cout << "Spinbox requested range width: " << value << std::endl;
    int64_t const actual_range = ui->openGLWidget->setRangeWidth(static_cast<int64_t>(value));
    std::cout << "Actual range width achieved: " << actual_range << std::endl;

    // Update the spinbox with the actual range width achieved (in case it was clamped)
    if (actual_range != value) {
        std::cout << "Range was clamped, updating spinbox to: " << actual_range << std::endl;
        updateXAxisSamples(static_cast<int>(actual_range));
    }
}

void DataViewer_Widget::updateXAxisSamples(int value) {
    ui->x_axis_samples->blockSignals(true);
    ui->x_axis_samples->setValue(value);
    ui->x_axis_samples->blockSignals(false);
}

void DataViewer_Widget::_updateGlobalScale(double scale) {
    ui->openGLWidget->setGlobalScale(static_cast<float>(scale));

    // Also update PlottingManager zoom factor
    if (_plotting_manager) {
        _plotting_manager->setGlobalZoom(static_cast<float>(scale));

        // Apply updated positions to all registered series
        auto analog_keys = _plotting_manager->getVisibleAnalogSeriesKeys();
        for (auto const & key: analog_keys) {
            _applyPlottingManagerAllocation(key);
        }
        auto event_keys = _plotting_manager->getVisibleDigitalEventSeriesKeys();
        for (auto const & key: event_keys) {
            _applyPlottingManagerAllocation(key);
        }
        auto interval_keys = _plotting_manager->getVisibleDigitalIntervalSeriesKeys();
        for (auto const & key: interval_keys) {
            _applyPlottingManagerAllocation(key);
        }

        // Trigger canvas update
        ui->openGLWidget->updateCanvas();
    }
}

void DataViewer_Widget::wheelEvent(QWheelEvent * event) {
    // Disable zooming while dragging intervals
    if (ui->openGLWidget->isDraggingInterval()) {
        return;
    }

    auto const numDegrees = static_cast<float>(event->angleDelta().y()) / 8.0f;
    auto const numSteps = numDegrees / 15.0f;

    auto const current_range = ui->x_axis_samples->value();

    float rangeFactor;
    if (_zoom_scaling_mode == ZoomScalingMode::Adaptive) {
        // Adaptive scaling: range factor is proportional to current range width
        // This makes adjustments more sensitive when zoomed in (small range), less sensitive when zoomed out (large range)
        rangeFactor = static_cast<float>(current_range) * 0.1f;// 10% of current range width

        // Clamp range factor to reasonable bounds
        rangeFactor = std::max(1.0f, std::min(rangeFactor, static_cast<float>(_time_frame->getTotalFrameCount()) / 100.0f));
    } else {
        // Fixed scaling (original behavior)
        rangeFactor = static_cast<float>(_time_frame->getTotalFrameCount()) / 10000.0f;
    }

    // Calculate range delta
    // Wheel up (positive numSteps) should zoom IN (decrease range width)
    // Wheel down (negative numSteps) should zoom OUT (increase range width)
    auto const range_delta = static_cast<int64_t>(-numSteps * rangeFactor);

    // Apply range delta and get the actual achieved range
    ui->openGLWidget->changeRangeWidth(range_delta);

    // Get the actual range that was achieved (may be different due to clamping)
    auto x_axis = ui->openGLWidget->getXAxis();
    auto const actual_range = static_cast<int>(x_axis.getEnd() - x_axis.getStart());

    // Update spinbox with the actual achieved range (not the requested range)
    updateXAxisSamples(actual_range);
    _updateLabels();
}

void DataViewer_Widget::_updateLabels() {
    auto x_axis = ui->openGLWidget->getXAxis();
    ui->neg_x_label->setText(QString::number(x_axis.getStart()));
    ui->pos_x_label->setText(QString::number(x_axis.getEnd()));
}

void DataViewer_Widget::_handleColorChanged(std::string const & feature_key, std::string const & hex_color) {
    // Update the color in the OpenGL widget display options (tree widget color management will be added later)

    auto const type = _data_manager->getType(feature_key);

    if (type == DM_DataType::Analog) {
        auto config = ui->openGLWidget->getAnalogConfig(feature_key);
        if (config.has_value()) {
            config.value()->hex_color = hex_color;
        }

    } else if (type == DM_DataType::DigitalEvent) {
        auto config = ui->openGLWidget->getDigitalEventConfig(feature_key);
        if (config.has_value()) {
            config.value()->hex_color = hex_color;
        }

    } else if (type == DM_DataType::DigitalInterval) {
        auto config = ui->openGLWidget->getDigitalIntervalConfig(feature_key);
        if (config.has_value()) {
            config.value()->hex_color = hex_color;
        }
    }

    // Trigger a redraw
    ui->openGLWidget->updateCanvas();

    std::cout << "Color changed for " << feature_key << " to " << hex_color << std::endl;
}

void DataViewer_Widget::_updateCoordinateDisplay(float time_coordinate, float canvas_y, QString const & series_info) {
    // Convert time coordinate to actual time using the time frame
    int const time_index = static_cast<int>(std::round(time_coordinate));
    int const actual_time = _time_frame->getTimeAtIndex(TimeFrameIndex(time_index));

    // Get canvas size for debugging
    auto [canvas_width, canvas_height] = ui->openGLWidget->getCanvasSize();

    // Use fixed-width formatting to prevent label resizing
    // Reserve space for reasonable max values (time: 10 digits, index: 10 digits, Y: 8 chars, canvas: 5x5 digits)
    QString coordinate_text;
    if (series_info.isEmpty()) {
        coordinate_text = QString("Time: %1  Index: %2  Y: %3  Canvas: %4x%5")
                                  .arg(actual_time, 10)           // Right-aligned, width 10
                                  .arg(time_index, 10)             // Right-aligned, width 10
                                  .arg(canvas_y, 8, 'f', 1)       // Right-aligned, width 8, 1 decimal
                                  .arg(canvas_width, 5)            // Right-aligned, width 5
                                  .arg(canvas_height, 5);          // Right-aligned, width 5
    } else {
        // For series info, still use fixed-width for numeric values but allow series info to vary
        coordinate_text = QString("Time: %1  Index: %2  %3  Canvas: %4x%5")
                                  .arg(actual_time, 10)
                                  .arg(time_index, 10)
                                  .arg(series_info, -30)           // Left-aligned, min width 30
                                  .arg(canvas_width, 5)
                                  .arg(canvas_height, 5);
    }

    ui->coordinate_label->setText(coordinate_text);
}

std::optional<NewAnalogTimeSeriesDisplayOptions *> DataViewer_Widget::getAnalogConfig(std::string const & key) const {
    return ui->openGLWidget->getAnalogConfig(key);
}

std::optional<NewDigitalEventSeriesDisplayOptions *> DataViewer_Widget::getDigitalEventConfig(std::string const & key) const {
    return ui->openGLWidget->getDigitalEventConfig(key);
}

std::optional<NewDigitalIntervalSeriesDisplayOptions *> DataViewer_Widget::getDigitalIntervalConfig(std::string const & key) const {
    return ui->openGLWidget->getDigitalIntervalConfig(key);
}

void DataViewer_Widget::_handleThemeChanged(int theme_index) {
    PlotTheme theme = (theme_index == 0) ? PlotTheme::Dark : PlotTheme::Light;
    ui->openGLWidget->setPlotTheme(theme);

    // Update coordinate label styling based on theme
    if (theme == PlotTheme::Dark) {
        ui->coordinate_label->setStyleSheet("background-color: rgba(0, 0, 0, 50); color: white; padding: 2px;");
    } else {
        ui->coordinate_label->setStyleSheet("background-color: rgba(255, 255, 255, 50); color: black; padding: 2px;");
    }

    std::cout << "Theme changed to: " << (theme_index == 0 ? "Dark" : "Light") << std::endl;
}

void DataViewer_Widget::_handleGridLinesToggled(bool enabled) {
    ui->openGLWidget->setGridLinesEnabled(enabled);
}

void DataViewer_Widget::_handleGridSpacingChanged(int spacing) {
    ui->openGLWidget->setGridSpacing(spacing);
}

void DataViewer_Widget::_handleVerticalSpacingChanged(double spacing) {
    ui->openGLWidget->setVerticalSpacing(static_cast<float>(spacing));

    // Also update PlottingManager vertical scale
    if (_plotting_manager) {
        // Convert spacing to a scale factor relative to default (0.1f)
        float const scale_factor = static_cast<float>(spacing) / 0.1f;
        _plotting_manager->setGlobalVerticalScale(scale_factor);

        // Apply updated positions to all registered series
        auto analog_keys = _plotting_manager->getVisibleAnalogSeriesKeys();
        for (auto const & key: analog_keys) {
            _applyPlottingManagerAllocation(key);
        }
        auto event_keys = _plotting_manager->getVisibleDigitalEventSeriesKeys();
        for (auto const & key: event_keys) {
            _applyPlottingManagerAllocation(key);
        }
        auto interval_keys = _plotting_manager->getVisibleDigitalIntervalSeriesKeys();
        for (auto const & key: interval_keys) {
            _applyPlottingManagerAllocation(key);
        }

        // Trigger canvas update
        ui->openGLWidget->updateCanvas();
    }
}

void DataViewer_Widget::_plotSelectedFeatureWithoutUpdate(std::string const & key) {
    std::cout << "Attempting to plot feature (batch): " << key << std::endl;

    if (key.empty()) {
        std::cerr << "Error: empty key in _plotSelectedFeatureWithoutUpdate" << std::endl;
        return;
    }

    if (!_data_manager) {
        std::cerr << "Error: null data manager in _plotSelectedFeatureWithoutUpdate" << std::endl;
        return;
    }

    // Get color from model
    std::string color = _feature_tree_model->getFeatureColor(key);
    std::cout << "Using color: " << color << " for series: " << key << std::endl;

    auto data_type = _data_manager->getType(key);
    std::cout << "Feature type: " << convert_data_type_to_string(data_type) << std::endl;

    if (data_type == DM_DataType::Analog) {
        auto series = _data_manager->getData<AnalogTimeSeries>(key);
        if (!series) {
            std::cerr << "Error: failed to get AnalogTimeSeries for key: " << key << std::endl;
            return;
        }

        auto time_key = _data_manager->getTimeKey(key);
        auto time_frame = _data_manager->getTime(time_key);
        if (!time_frame) {
            std::cerr << "Error: failed to get TimeFrame for key: " << key << std::endl;
            return;
        }

        // Register with plotting manager for later allocation (single call)
        _plotting_manager->addAnalogSeries(key, series, time_frame, color);
        ui->openGLWidget->addAnalogTimeSeries(key, series, time_frame, color);

    } else if (data_type == DM_DataType::DigitalEvent) {
        auto series = _data_manager->getData<DigitalEventSeries>(key);
        if (!series) {
            std::cerr << "Error: failed to get DigitalEventSeries for key: " << key << std::endl;
            return;
        }

        auto time_key = _data_manager->getTimeKey(key);
        auto time_frame = _data_manager->getTime(time_key);
        if (!time_frame) {
            std::cerr << "Error: failed to get TimeFrame for key: " << key << std::endl;
            return;
        }
        _plotting_manager->addDigitalEventSeries(key, series, time_frame, color);
        ui->openGLWidget->addDigitalEventSeries(key, series, time_frame, color);

    } else if (data_type == DM_DataType::DigitalInterval) {
        auto series = _data_manager->getData<DigitalIntervalSeries>(key);
        if (!series) {
            std::cerr << "Error: failed to get DigitalIntervalSeries for key: " << key << std::endl;
            return;
        }

        auto time_key = _data_manager->getTimeKey(key);
        auto time_frame = _data_manager->getTime(time_key);
        if (!time_frame) {
            std::cerr << "Error: failed to get TimeFrame for key: " << key << std::endl;
            return;
        }
        _plotting_manager->addDigitalIntervalSeries(key, series, time_frame, color);
        ui->openGLWidget->addDigitalIntervalSeries(key, series, time_frame, color);

    } else {
        std::cout << "Feature type not supported: " << convert_data_type_to_string(data_type) << std::endl;
        return;
    }

    // Note: No canvas update triggered - this is for batch operations
    std::cout << "Successfully added series to OpenGL widget (batch mode)" << std::endl;
}

void DataViewer_Widget::_removeSelectedFeatureWithoutUpdate(std::string const & key) {
    std::cout << "Attempting to remove feature (batch): " << key << std::endl;

    if (key.empty()) {
        std::cerr << "Error: empty key in _removeSelectedFeatureWithoutUpdate" << std::endl;
        return;
    }

    if (!_data_manager) {
        std::cerr << "Error: null data manager in _removeSelectedFeatureWithoutUpdate" << std::endl;
        return;
    }

    auto data_type = _data_manager->getType(key);

    // Also unregister from the plotting manager so counts and ordering stay consistent
    if (_plotting_manager) {
        if (data_type == DM_DataType::Analog) {
            (void) _plotting_manager->removeAnalogSeries(key);
        } else if (data_type == DM_DataType::DigitalEvent) {
            (void) _plotting_manager->removeDigitalEventSeries(key);
        } else if (data_type == DM_DataType::DigitalInterval) {
            (void) _plotting_manager->removeDigitalIntervalSeries(key);
        }
    }

    if (data_type == DM_DataType::Analog) {
        ui->openGLWidget->removeAnalogTimeSeries(key);
    } else if (data_type == DM_DataType::DigitalEvent) {
        ui->openGLWidget->removeDigitalEventSeries(key);
    } else if (data_type == DM_DataType::DigitalInterval) {
        ui->openGLWidget->removeDigitalIntervalSeries(key);
    } else {
        std::cout << "Feature type not supported for removal: " << convert_data_type_to_string(data_type) << std::endl;
        return;
    }

    // Note: No canvas update triggered - this is for batch operations
    std::cout << "Successfully removed series from OpenGL widget (batch mode)" << std::endl;
}

void DataViewer_Widget::_calculateOptimalScaling(std::vector<std::string> const & group_keys) {
    if (group_keys.empty()) {
        return;
    }

    std::cout << "Calculating optimal scaling for " << group_keys.size() << " analog channels..." << std::endl;

    // Get current canvas dimensions
    auto [canvas_width, canvas_height] = ui->openGLWidget->getCanvasSize();
    std::cout << "Canvas size: " << canvas_width << "x" << canvas_height << " pixels" << std::endl;

    // Count total number of currently visible analog series (including the new group)
    int total_visible_analog_series = static_cast<int>(group_keys.size());

    // Add any other already visible analog series
    auto all_keys = _data_manager->getAllKeys();
    for (auto const & key: all_keys) {
        if (_data_manager->getType(key) == DM_DataType::Analog) {
            // Check if this key is already in our group (avoid double counting)
            bool in_group = std::find(group_keys.begin(), group_keys.end(), key) != group_keys.end();
            if (!in_group) {
                // Check if this series is currently visible
                auto config = ui->openGLWidget->getAnalogConfig(key);
                if (config.has_value() && config.value()->is_visible) {
                    total_visible_analog_series++;
                }
            }
        }
    }

    std::cout << "Total visible analog series (including new group): " << total_visible_analog_series << std::endl;

    if (total_visible_analog_series <= 0) {
        return;// No series to scale
    }

    // Calculate optimal vertical spacing
    // Leave some margin at top and bottom (10% each = 20% total)
    float const effective_height = static_cast<float>(canvas_height) * 0.8f;
    float const optimal_spacing = effective_height / static_cast<float>(total_visible_analog_series);

    // Convert to normalized coordinates (OpenGL widget uses normalized spacing)
    // Assuming the widget's view height is typically around 2.0 units in normalized coordinates
    float const normalized_spacing = (optimal_spacing / static_cast<float>(canvas_height)) * 2.0f;

    // Clamp to reasonable bounds
    float const min_spacing = 0.01f;
    float const max_spacing = 1.0f;
    float const final_spacing = std::clamp(normalized_spacing, min_spacing, max_spacing);

    std::cout << "Calculated spacing: " << optimal_spacing << " pixels -> "
              << final_spacing << " normalized units" << std::endl;

    // Calculate optimal global gain based on standard deviations
    std::vector<float> std_devs;
    std_devs.reserve(group_keys.size());

    for (auto const & key: group_keys) {
        auto series = _data_manager->getData<AnalogTimeSeries>(key);
        if (series) {
            float const std_dev = calculate_std_dev_approximate(*series);
            std_devs.push_back(std_dev);
            std::cout << "Series " << key << " std dev: " << std_dev << std::endl;
        }
    }

    if (!std_devs.empty()) {
        // Use the median standard deviation as reference for scaling
        std::sort(std_devs.begin(), std_devs.end());
        float const median_std_dev = std_devs[std_devs.size() / 2];

        // Calculate optimal global scale
        // Target: each series should use about 60% of its allocated vertical space
        float const target_amplitude_fraction = 0.6f;
        float const target_amplitude_in_pixels = optimal_spacing * target_amplitude_fraction;

        // Convert to normalized coordinates (3 standard deviations should fit in target amplitude)
        float const target_amplitude_normalized = (target_amplitude_in_pixels / static_cast<float>(canvas_height)) * 2.0f;
        float const three_sigma_target = target_amplitude_normalized;

        // Calculate scale factor needed
        float const optimal_global_scale = three_sigma_target / (3.0f * median_std_dev);

        // Clamp to reasonable bounds
        float const min_scale = 0.1f;
        float const max_scale = 100.0f;
        float const final_scale = std::clamp(optimal_global_scale, min_scale, max_scale);

        std::cout << "Median std dev: " << median_std_dev
                  << ", target amplitude: " << target_amplitude_in_pixels << " pixels"
                  << ", optimal global scale: " << final_scale << std::endl;

        // Apply the calculated settings
        ui->vertical_spacing->setValue(static_cast<double>(final_spacing));
        ui->global_zoom->setValue(static_cast<double>(final_scale));

        std::cout << "Applied auto-scaling: vertical spacing = " << final_spacing
                  << ", global scale = " << final_scale << std::endl;

    } else {
        // If we can't calculate standard deviations, just apply spacing
        ui->vertical_spacing->setValue(static_cast<double>(final_spacing));
        std::cout << "Applied auto-spacing only: vertical spacing = " << final_spacing << std::endl;
    }
}

void DataViewer_Widget::_calculateOptimalEventSpacing(std::vector<std::string> const & group_keys) {
    if (group_keys.empty()) {
        return;
    }

    std::cout << "Calculating optimal event spacing for " << group_keys.size() << " digital event series..." << std::endl;

    // Get current canvas dimensions
    auto [canvas_width, canvas_height] = ui->openGLWidget->getCanvasSize();
    std::cout << "Canvas size: " << canvas_width << "x" << canvas_height << " pixels" << std::endl;

    // Count total number of currently visible digital event series (including the new group)
    int total_visible_event_series = static_cast<int>(group_keys.size());

    // Add any other already visible digital event series
    auto all_keys = _data_manager->getAllKeys();
    for (auto const & key: all_keys) {
        if (_data_manager->getType(key) == DM_DataType::DigitalEvent) {
            // Check if this key is already in our group (avoid double counting)
            bool const in_group = std::find(group_keys.begin(), group_keys.end(), key) != group_keys.end();
            if (!in_group) {
                // Check if this series is currently visible
                auto config = ui->openGLWidget->getDigitalEventConfig(key);
                if (config.has_value() && config.value()->is_visible) {
                    total_visible_event_series++;
                }
            }
        }
    }

    std::cout << "Total visible digital event series (including new group): " << total_visible_event_series << std::endl;

    if (total_visible_event_series <= 0) {
        return;// No series to scale
    }

    // Calculate optimal vertical spacing
    // Leave some margin at top and bottom (10% each = 20% total)
    float const effective_height = static_cast<float>(canvas_height) * 0.8f;
    float const optimal_spacing = effective_height / static_cast<float>(total_visible_event_series);

    // Convert to normalized coordinates (OpenGL widget uses normalized spacing)
    // Assuming the widget's view height is typically around 2.0 units in normalized coordinates
    float const normalized_spacing = (optimal_spacing / static_cast<float>(canvas_height)) * 2.0f;

    // Clamp to reasonable bounds
    float const min_spacing = 0.01f;
    float const max_spacing = 1.0f;
    float const final_spacing = std::clamp(normalized_spacing, min_spacing, max_spacing);

    // Calculate optimal event height (keep events clearly within their lane)
    // Use a conservative fraction of spacing so multiple stacked series remain visually distinct
    float const optimal_event_height = std::min(final_spacing * 0.3f, 0.2f);
    float const min_height = 0.01f;
    float const max_height = 0.5f;
    float const final_height = std::clamp(optimal_event_height, min_height, max_height);

    std::cout << "Calculated spacing: " << optimal_spacing << " pixels -> "
              << final_spacing << " normalized units" << std::endl;
    std::cout << "Calculated event height: " << final_height << " normalized units" << std::endl;

    // Apply the calculated settings to all event series in the group
    for (auto const & key: group_keys) {
        auto config = ui->openGLWidget->getDigitalEventConfig(key);
        if (config.has_value()) {
            config.value()->vertical_spacing = final_spacing;
            config.value()->event_height = final_height;
            config.value()->display_mode = EventDisplayMode::Stacked;// Ensure stacked mode
        }
    }

    std::cout << "Applied auto-calculated event spacing: spacing = " << final_spacing
              << ", height = " << final_height << std::endl;
}

void DataViewer_Widget::autoArrangeVerticalSpacing() {
    std::cout << "DataViewer_Widget: Auto-arranging with plotting manager..." << std::endl;

    // Update dimensions first
    _updatePlottingManagerDimensions();

    // Apply new allocations to all registered series
    auto analog_keys = _plotting_manager->getVisibleAnalogSeriesKeys();
    auto event_keys = _plotting_manager->getVisibleDigitalEventSeriesKeys();
    auto interval_keys = _plotting_manager->getVisibleDigitalIntervalSeriesKeys();

    for (auto const & key: analog_keys) {
        _applyPlottingManagerAllocation(key);
    }
    for (auto const & key: event_keys) {
        _applyPlottingManagerAllocation(key);
    }
    for (auto const & key: interval_keys) {
        _applyPlottingManagerAllocation(key);
    }

    // Calculate and apply optimal scaling to fill the canvas
    _autoFillCanvas();

    // Update OpenGL widget view bounds based on content height
    _updateViewBounds();

    // Trigger canvas update to show new positions
    ui->openGLWidget->updateCanvas();

    auto total_keys = analog_keys.size() + event_keys.size() + interval_keys.size();
    std::cout << "DataViewer_Widget: Auto-arrange completed for " << total_keys << " series" << std::endl;
}

void DataViewer_Widget::_updateViewBounds() {
    if (!_plotting_manager) {
        return;
    }

    // PlottingManager uses normalized coordinates, so view bounds are typically -1 to +1
    // For now, use standard bounds but this enables future enhancement
    std::cout << "DataViewer_Widget: Using standard view bounds with PlottingManager" << std::endl;
}

std::string DataViewer_Widget::_convertDataType(DM_DataType dm_type) const {
    switch (dm_type) {
        case DM_DataType::Analog:
            return "Analog";
        case DM_DataType::DigitalEvent:
            return "DigitalEvent";
        case DM_DataType::DigitalInterval:
            return "DigitalInterval";
        default:
            // For unsupported types, default to Analog
            // This should be rare in practice given our type filters
            std::cerr << "Warning: Unsupported data type " << convert_data_type_to_string(dm_type)
                      << " defaulting to Analog for plotting manager" << std::endl;
            return "Analog";
    }
}

void DataViewer_Widget::_updatePlottingManagerDimensions() {
    if (!_plotting_manager) {
        return;
    }

    // Get current canvas dimensions from OpenGL widget
    auto [canvas_width, canvas_height] = ui->openGLWidget->getCanvasSize();

    // PlottingManager works in normalized device coordinates, so no specific dimension update needed
    // But we could update viewport bounds if needed in the future

    std::cout << "DataViewer_Widget: Updated plotting manager dimensions: "
              << canvas_width << "x" << canvas_height << " pixels" << std::endl;
}

void DataViewer_Widget::_applyPlottingManagerAllocation(std::string const & series_key) {
    if (!_plotting_manager) {
        return;
    }

    auto data_type = _data_manager->getType(series_key);

    std::cout << "DataViewer_Widget: Applying plotting manager allocation for '" << series_key << "'" << std::endl;

    // For now, use a basic implementation that will be enhanced when we update OpenGLWidget
    // The main goal is to get the compilation working first

    // Apply positioning based on data type
    if (data_type == DM_DataType::Analog) {
        auto config = ui->openGLWidget->getAnalogConfig(series_key);
        if (config.has_value()) {
            float yc, yh;
            if (_plotting_manager->getAnalogSeriesAllocationForKey(series_key, yc, yh)) {
                config.value()->allocated_y_center = yc;
                config.value()->allocated_height = yh;
            }
        }

    } else if (data_type == DM_DataType::DigitalEvent) {
        auto config = ui->openGLWidget->getDigitalEventConfig(series_key);
        if (config.has_value()) {
            // Basic allocation - will be properly implemented when OpenGL widget is updated
            std::cout << "  Applied basic allocation to event '" << series_key << "'" << std::endl;
        }

    } else if (data_type == DM_DataType::DigitalInterval) {
        auto config = ui->openGLWidget->getDigitalIntervalConfig(series_key);
        if (config.has_value()) {
            // Basic allocation - will be properly implemented when OpenGL widget is updated
            std::cout << "  Applied basic allocation to interval '" << series_key << "'" << std::endl;
        }
    }
}

// ===== Context menu and configuration handling =====
void DataViewer_Widget::_showGroupContextMenu(std::string const & group_name, QPoint const & global_pos) {
    QMenu menu;
    QMenu * loadMenu = menu.addMenu("Load configuration");
    QAction * loadSpikeSorter = loadMenu->addAction("spikesorter configuration");
    QAction * clearConfig = menu.addAction("Clear configuration");

    connect(loadSpikeSorter, &QAction::triggered, this, [this, group_name]() {
        _loadSpikeSorterConfigurationForGroup(QString::fromStdString(group_name));
    });
    connect(clearConfig, &QAction::triggered, this, [this, group_name]() {
        _clearConfigurationForGroup(QString::fromStdString(group_name));
    });

    menu.exec(global_pos);
}

void DataViewer_Widget::_loadSpikeSorterConfigurationForGroup(QString const & group_name) {
    // For now, use a test constant string or file dialog; here we open a file dialog
    QString path = QFileDialog::getOpenFileName(this, QString("Load spikesorter configuration for %1").arg(group_name), QString(), "Text Files (*.txt *.cfg *.conf);;All Files (*)");
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QByteArray data = file.readAll();
    auto positions = _parseSpikeSorterConfig(data.toStdString());
    if (positions.empty()) return;
    _plotting_manager->loadAnalogSpikeSorterConfiguration(group_name.toStdString(), positions);

    // Re-apply allocation to visible analog keys and update
    auto analog_keys = _plotting_manager->getVisibleAnalogSeriesKeys();
    for (auto const & key : analog_keys) {
        _applyPlottingManagerAllocation(key);
    }
    ui->openGLWidget->updateCanvas();
}

void DataViewer_Widget::_clearConfigurationForGroup(QString const & group_name) {
    _plotting_manager->clearAnalogGroupConfiguration(group_name.toStdString());
    auto analog_keys = _plotting_manager->getVisibleAnalogSeriesKeys();
    for (auto const & key : analog_keys) {
        _applyPlottingManagerAllocation(key);
    }
    ui->openGLWidget->updateCanvas();
}

std::vector<PlottingManager::AnalogGroupChannelPosition> DataViewer_Widget::_parseSpikeSorterConfig(std::string const & text) {
    std::vector<PlottingManager::AnalogGroupChannelPosition> out;
    std::istringstream ss(text);
    std::string line;
    bool first = true;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        if (first) { first = false; continue; } // skip header row (electrode name)
        std::istringstream ls(line);
        int row = 0; int ch = 0; float x = 0.0f; float y = 0.0f;
        if (!(ls >> row >> ch >> x >> y)) continue;
        // SpikeSorter is 1-based; convert to 0-based for our program
        if (ch > 0) ch -= 1;
        PlottingManager::AnalogGroupChannelPosition p; p.channel_id = ch; p.x = x; p.y = y;
        out.push_back(p);
    }
    return out;
}

void DataViewer_Widget::_loadSpikeSorterConfigurationFromText(QString const & group_name, QString const & text) {
    auto positions = _parseSpikeSorterConfig(text.toStdString());
    if (positions.empty()) {
        std::cout << "No positions found in spike sorter configuration" << std::endl;
        return;
    }
    _plotting_manager->loadAnalogSpikeSorterConfiguration(group_name.toStdString(), positions);
    auto analog_keys = _plotting_manager->getVisibleAnalogSeriesKeys();
    for (auto const & key : analog_keys) {
        _applyPlottingManagerAllocation(key);
    }
    ui->openGLWidget->updateCanvas();
}

void DataViewer_Widget::_autoFillCanvas() {
    std::cout << "DataViewer_Widget: Auto-filling canvas with PlottingManager..." << std::endl;

    if (!_plotting_manager) {
        std::cout << "No plotting manager available" << std::endl;
        return;
    }

    // Get current canvas dimensions
    auto [canvas_width, canvas_height] = ui->openGLWidget->getCanvasSize();
    std::cout << "Canvas size: " << canvas_width << "x" << canvas_height << " pixels" << std::endl;

    // Count visible series using PlottingManager
    auto analog_keys = _plotting_manager->getVisibleAnalogSeriesKeys();
    auto event_keys = _plotting_manager->getVisibleDigitalEventSeriesKeys();
    auto interval_keys = _plotting_manager->getVisibleDigitalIntervalSeriesKeys();

    int visible_analog_count = static_cast<int>(analog_keys.size());
    int visible_event_count = static_cast<int>(event_keys.size());
    int visible_interval_count = static_cast<int>(interval_keys.size());

    int total_visible = visible_analog_count + visible_event_count + visible_interval_count;
    std::cout << "Visible series: " << visible_analog_count << " analog, "
              << visible_event_count << " events, " << visible_interval_count
              << " intervals (total: " << total_visible << ")" << std::endl;

    if (total_visible == 0) {
        std::cout << "No visible series to auto-scale" << std::endl;
        return;
    }

    // Calculate optimal vertical spacing to fill canvas
    // Use 90% of canvas height, leaving 5% margin at top and bottom
    float const usable_height = static_cast<float>(canvas_height) * 0.9f;
    float const optimal_spacing_pixels = usable_height / static_cast<float>(total_visible);

    // Convert to normalized coordinates (assuming 2.0 total normalized height)
    float const optimal_spacing_normalized = (optimal_spacing_pixels / static_cast<float>(canvas_height)) * 2.0f;

    // Clamp to reasonable bounds
    float const min_spacing = 0.02f;
    float const max_spacing = 1.5f;
    float const final_spacing = std::clamp(optimal_spacing_normalized, min_spacing, max_spacing);

    std::cout << "Calculated optimal spacing: " << optimal_spacing_pixels << " pixels -> "
              << final_spacing << " normalized units" << std::endl;

    // Apply the calculated vertical spacing
    ui->vertical_spacing->setValue(static_cast<double>(final_spacing));

    // Calculate and apply optimal event heights for digital event series
    if (visible_event_count > 0) {
        // Calculate event height conservatively to avoid near full-lane rendering
        // Use 30% of the spacing and cap at 0.2 to keep consistent scale across zooms
        float const optimal_event_height = std::min(final_spacing * 0.3f, 0.2f);

        std::cout << "Calculated optimal event height: " << optimal_event_height << " normalized units" << std::endl;

        // Apply optimal height to all visible digital event series
        for (auto const & key: event_keys) {
            auto config = ui->openGLWidget->getDigitalEventConfig(key);
            if (config.has_value() && config.value()->is_visible) {
                config.value()->event_height = optimal_event_height;
                config.value()->display_mode = EventDisplayMode::Stacked;// Ensure stacked mode
                std::cout << "  Applied event height " << optimal_event_height
                          << " to series '" << key << "'" << std::endl;
            }
        }
    }

    // Calculate and apply optimal interval heights for digital interval series
    if (visible_interval_count > 0) {
        // Calculate optimal interval height to fill most of the allocated space
        // Use 80% of the spacing to leave some visual separation between intervals
        float const optimal_interval_height = final_spacing * 0.8f;

        std::cout << "Calculated optimal interval height: " << optimal_interval_height << " normalized units" << std::endl;

        // Apply optimal height to all visible digital interval series
        for (auto const & key: interval_keys) {
            auto config = ui->openGLWidget->getDigitalIntervalConfig(key);
            if (config.has_value() && config.value()->is_visible) {
                config.value()->interval_height = optimal_interval_height;
                std::cout << "  Applied interval height " << optimal_interval_height
                          << " to series '" << key << "'" << std::endl;
            }
        }
    }

    // Calculate optimal global scale for analog series to use their allocated space effectively
    if (visible_analog_count > 0) {
        // Sample a few analog series to estimate appropriate scaling
        std::vector<float> sample_std_devs;
        sample_std_devs.reserve(std::min(5, visible_analog_count));// Sample up to 5 series

        int sampled = 0;
        for (auto const & key: analog_keys) {
            if (sampled >= 5) break;

            auto config = ui->openGLWidget->getAnalogConfig(key);
            if (config.has_value() && config.value()->is_visible) {
                auto series = _data_manager->getData<AnalogTimeSeries>(key);
                if (series) {
                    float std_dev = calculate_std_dev_approximate(*series);
                    if (std_dev > 0.0f) {
                        sample_std_devs.push_back(std_dev);
                        sampled++;
                    }
                }
            }
        }

        if (!sample_std_devs.empty()) {
            // Use median standard deviation for scaling calculation
            std::sort(sample_std_devs.begin(), sample_std_devs.end());
            float median_std_dev = sample_std_devs[sample_std_devs.size() / 2];

            // Calculate scale so that ±3 standard deviations use ~60% of allocated space
            float const target_amplitude_fraction = 0.6f;
            float const target_amplitude_pixels = optimal_spacing_pixels * target_amplitude_fraction;
            float const target_amplitude_normalized = (target_amplitude_pixels / static_cast<float>(canvas_height)) * 2.0f;

            // For ±3σ coverage
            float const three_sigma_coverage = target_amplitude_normalized;
            float const optimal_global_scale = three_sigma_coverage / (6.0f * median_std_dev);

            // Clamp to reasonable bounds
            float const min_scale = 0.01f;
            float const max_scale = 100.0f;
            float const final_scale = std::clamp(optimal_global_scale, min_scale, max_scale);

            std::cout << "Calculated optimal global scale: median_std_dev=" << median_std_dev
                      << ", target_amplitude=" << target_amplitude_pixels << " pixels"
                      << ", final_scale=" << final_scale << std::endl;

            // Apply the calculated global scale
            ui->global_zoom->setValue(static_cast<double>(final_scale));
        }
    }

    std::cout << "Auto-fill canvas completed" << std::endl;
}

void DataViewer_Widget::cleanupDeletedData() {
    if (!_data_manager) {
        return;
    }

    // Collect keys that no longer exist in DataManager
    std::vector<std::string> keys_to_cleanup;

    if (_plotting_manager) {
        auto analog_keys = _plotting_manager->getVisibleAnalogSeriesKeys();
        for (auto const & key: analog_keys) {
            if (!_data_manager->getData<AnalogTimeSeries>(key)) {
                keys_to_cleanup.push_back(key);
            }
        }
        auto event_keys = _plotting_manager->getVisibleDigitalEventSeriesKeys();
        for (auto const & key: event_keys) {
            if (!_data_manager->getData<DigitalEventSeries>(key)) {
                keys_to_cleanup.push_back(key);
            }
        }
        auto interval_keys = _plotting_manager->getVisibleDigitalIntervalSeriesKeys();
        for (auto const & key: interval_keys) {
            if (!_data_manager->getData<DigitalIntervalSeries>(key)) {
                keys_to_cleanup.push_back(key);
            }
        }
    }

    if (keys_to_cleanup.empty()) {
        return;
    }

    // De-duplicate keys in case the same key appears in multiple lists
    std::sort(keys_to_cleanup.begin(), keys_to_cleanup.end());
    keys_to_cleanup.erase(std::unique(keys_to_cleanup.begin(), keys_to_cleanup.end()), keys_to_cleanup.end());

    // Post cleanup to OpenGLWidget's thread safely
    QPointer<OpenGLWidget> glw = ui ? ui->openGLWidget : nullptr;
    if (glw) {
        QMetaObject::invokeMethod(glw, [glw, keys = keys_to_cleanup]() {
            if (!glw) return;
            for (auto const & key : keys) {
                glw->removeAnalogTimeSeries(key);
                glw->removeDigitalEventSeries(key);
                glw->removeDigitalIntervalSeries(key);
            } }, Qt::QueuedConnection);
    }

    // Remove from PlottingManager defensively (all types) on our thread
    if (_plotting_manager) {
        for (auto const & key: keys_to_cleanup) {
            (void) _plotting_manager->removeAnalogSeries(key);
            (void) _plotting_manager->removeDigitalEventSeries(key);
            (void) _plotting_manager->removeDigitalIntervalSeries(key);
        }
    }

    // Re-arrange remaining data
    autoArrangeVerticalSpacing();
}

void DataViewer_Widget::_hidePropertiesPanel() {
    // Save current splitter sizes before hiding
    _saved_splitter_sizes = ui->main_splitter->sizes();
    
    // Collapse the properties panel to 0 width
    ui->main_splitter->setSizes({0, ui->main_splitter->sizes()[1]});
    
    // Hide the properties panel and show the reveal button
    ui->properties_container->hide();
    ui->show_properties_button->show();
    
    _properties_panel_collapsed = true;
    
    std::cout << "Properties panel hidden" << std::endl;
    
    // Trigger a canvas update to adjust to new size
    ui->openGLWidget->update();
}

void DataViewer_Widget::_showPropertiesPanel() {
    // Show the properties panel
    ui->properties_container->show();
    
    // Restore saved splitter sizes
    if (!_saved_splitter_sizes.isEmpty()) {
        ui->main_splitter->setSizes(_saved_splitter_sizes);
    } else {
        // Default sizes if no saved sizes (320px for properties, rest for plot)
        ui->main_splitter->setSizes({320, 1000});
    }
    
    // Hide the reveal button
    ui->show_properties_button->hide();
    
    _properties_panel_collapsed = false;
    
    std::cout << "Properties panel shown" << std::endl;
    
    // Trigger a canvas update to adjust to new size
    ui->openGLWidget->update();
}

void DataViewer_Widget::_exportToSVG() {
    std::cout << "SVG Export initiated" << std::endl;

    // Get save file path from user
    QString const fileName = QFileDialog::getSaveFileName(
        this,
        tr("Export Plot to SVG"),
        QString(),
        tr("SVG Files (*.svg);;All Files (*)"));

    if (fileName.isEmpty()) {
        std::cout << "SVG Export cancelled by user" << std::endl;
        return;
    }

    try {
        // Create SVG exporter with current plot state
        SVGExporter exporter(ui->openGLWidget, _plotting_manager.get());

        // Configure scalebar if requested
        if (ui->svg_scalebar_checkbox->isChecked()) {
            int const scalebar_length = ui->scalebar_length_spinbox->value();
            exporter.enableScalebar(true, scalebar_length);
        }

        // Generate SVG document
        QString const svg_content = exporter.exportToSVG();

        // Write to file
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::critical(
                this,
                tr("Export Failed"),
                tr("Could not open file for writing:\n%1").arg(fileName));
            std::cerr << "Failed to open file: " << fileName.toStdString() << std::endl;
            return;
        }

        QTextStream out(&file);
        out << svg_content;
        file.close();

        std::cout << "SVG Export successful: " << fileName.toStdString() << std::endl;

        // Show success message
        QMessageBox::information(
            this,
            tr("Export Successful"),
            tr("Plot exported to:\n%1\n\nCanvas size: %2x%3")
                .arg(fileName)
                .arg(exporter.getCanvasWidth())
                .arg(exporter.getCanvasHeight()));

    } catch (std::exception const & e) {
        QMessageBox::critical(
            this,
            tr("Export Failed"),
            tr("An error occurred during export:\n%1").arg(e.what()));
        std::cerr << "SVG Export failed: " << e.what() << std::endl;
    }
}
