#include "EventPlotWidget.hpp"

#include "Core/EventPlotState.hpp"
#include "CorePlotting/CoordinateTransform/AxisMapping.hpp"
#include "CorePlotting/CoordinateTransform/ViewState.hpp"
#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "PlotDataExport/RasterCSVExport.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWithRangeControls.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisState.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWidget.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"
#include "Rendering/EventPlotOpenGLWidget.hpp"
#include "StateManagement/AppFileDialog.hpp"

#include <QFile>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QResizeEvent>
#include <QTextStream>
#include <QVBoxLayout>
#include <set>
#include <utility>

#include "ui_EventPlotWidget.h"

EventPlotWidget::EventPlotWidget(std::shared_ptr<DataManager> data_manager,
                                 QWidget * parent)
    : QWidget(parent),
      _data_manager(std::move(std::move(data_manager))),
      ui(new Ui::EventPlotWidget),
      _opengl_widget(nullptr),
      _axis_widget(nullptr),
      _range_controls(nullptr),
      _vertical_axis_widget(nullptr),
      _vertical_range_controls(nullptr),
      _vertical_axis_state(nullptr) {
    ui->setupUi(this);

    // Create horizontal layout for vertical axis + OpenGL widget
    auto * horizontal_layout = new QHBoxLayout();
    horizontal_layout->setSpacing(0);
    horizontal_layout->setContentsMargins(0, 0, 0, 0);

    // Create vertical axis state (not serialized in EventPlotState since Y-axis is trial-based)
    _vertical_axis_state = std::make_unique<VerticalAxisState>(this);

    // Create combined vertical axis widget with range controls using factory
    // Range controls will be created in the properties widget
    auto vertical_axis_with_controls = createVerticalAxisWithRangeControls(
            _vertical_axis_state.get(), this, nullptr);
    _vertical_axis_widget = vertical_axis_with_controls.axis_widget;
    _vertical_range_controls = vertical_axis_with_controls.range_controls;
    _vertical_axis_state->setRange(0.0, 0.0);// Will be updated when trials are loaded
    horizontal_layout->addWidget(_vertical_axis_widget);

    // Create and add the OpenGL widget
    _opengl_widget = new EventPlotOpenGLWidget(this);
    _opengl_widget->setDataManager(_data_manager);
    horizontal_layout->addWidget(_opengl_widget, 1);// Stretch factor 1

    // Create vertical layout for horizontal layout + time axis
    auto * vertical_layout = new QVBoxLayout();
    vertical_layout->setSpacing(0);
    vertical_layout->setContentsMargins(0, 0, 0, 0);
    vertical_layout->addLayout(horizontal_layout, 1);// Stretch factor 1

    // Time axis widget and controls will be created in setState()
    // when we have access to the EventPlotState's RelativeTimeAxisState
    _axis_widget = nullptr;
    _range_controls = nullptr;

    // Replace the main layout
    QLayout * old_layout = layout();

    delete old_layout;

    setLayout(vertical_layout);

    // Forward signals from OpenGL widget
    connect(_opengl_widget, &EventPlotOpenGLWidget::eventDoubleClicked,
            this, [this](int64_t absolute_time, QString const & series_key) {
                // Get the TimeFrame from the series via DataManager
                std::shared_ptr<TimeFrame> time_frame;
                if (_data_manager && !series_key.isEmpty()) {
                    auto time_key = _data_manager->getTimeKey(series_key.toStdString());
                    if (!time_key.empty()) {
                        time_frame = _data_manager->getTime(time_key);
                    }
                }
                // The OpenGL widget computes alignment in absolute time units,
                // so we need to convert back to a TimeFrameIndex via the TimeFrame.
                TimeFrameIndex frame_index(absolute_time);
                if (time_frame) {
                    frame_index = time_frame->getIndexAtTime(static_cast<float>(absolute_time));
                }
                emit timePositionSelected(TimePosition(frame_index, time_frame));
            });

    // Forward event selection signal
    connect(_opengl_widget, &EventPlotOpenGLWidget::eventSelected,
            this, &EventPlotWidget::eventSelected);

    // Connect trial count changes to cache the count
    // The actual range is computed by the RangeGetter set up in setState()
    connect(_opengl_widget, &EventPlotOpenGLWidget::trialCountChanged,
            this, [this](size_t count) {
                _trial_count = count;
                if (_vertical_axis_widget) {
                    _vertical_axis_widget->update();
                }
            });
}

EventPlotWidget::~EventPlotWidget() {
    _unregisterAllDataCallbacks();
    if (_data_manager && _dm_observer_id != -1) {
        _data_manager->removeObserver(_dm_observer_id);
    }
    delete ui;
}

void EventPlotWidget::setState(std::shared_ptr<EventPlotState> state) {
    // Clean up old callbacks before switching state
    _unregisterAllDataCallbacks();

    _state = std::move(state);

    if (_opengl_widget) {
        _opengl_widget->setState(_state);
    }

    if (!_state) {
        return;
    }

    createTimeAxisIfNeeded();
    wireTimeAxis();
    wireVerticalAxis();
    connectViewChangeSignals();

    // Register data-change callbacks for existing plot event keys and alignment key
    _syncDataCallbacks();

    // Track when plot events are added/removed to manage callbacks
    connect(_state.get(), &EventPlotState::plotEventAdded,
            this, [this](QString const & /* event_name */) {
                _syncDataCallbacks();
            });
    connect(_state.get(), &EventPlotState::plotEventRemoved,
            this, [this](QString const & /* event_name */) {
                _syncDataCallbacks();
            });

    // Track alignment key changes to swap callback
    connect(_state.get(), &EventPlotState::alignmentEventKeyChanged,
            this, [this](QString const & /* key */) {
                _syncDataCallbacks();
            });

    // Register DataManager-level observer to detect key removal
    if (_data_manager && _dm_observer_id == -1) {
        _dm_observer_id = _data_manager->addObserver([this]() {
            _pruneRemovedKeys();
        }, "EventPlotWidget");
    }

    // Initialize axis ranges from current view state
    syncTimeAxisRange();
    syncVerticalAxisRange();
}

// ---------------------------------------------------------------------------
// setState decomposition — each method handles one axis or concern
// ---------------------------------------------------------------------------

void EventPlotWidget::createTimeAxisIfNeeded() {
    if (_axis_widget) {
        return;// Already created
    }

    auto * time_axis_state = _state->relativeTimeAxisState();
    if (!time_axis_state) {
        return;
    }

    auto result = createRelativeTimeAxisWithRangeControls(
            time_axis_state, this, nullptr);
    _axis_widget = result.axis_widget;
    _range_controls = result.range_controls;

    if (auto * vbox = qobject_cast<QVBoxLayout *>(layout())) {
        vbox->addWidget(_axis_widget);
    }
}

void EventPlotWidget::wireTimeAxis() {
    if (!_axis_widget) {
        return;
    }

    _axis_widget->setAxisMapping(CorePlotting::relativeTimeAxis());
    _axis_widget->setAlignmentTarget(_opengl_widget);

    _axis_widget->setViewStateGetter([this]() {
        if (!_state || !_opengl_widget) {
            return CorePlotting::ViewState{};
        }
        auto vs = CorePlotting::toRuntimeViewState(
                _state->viewState(),
                _opengl_widget->width(),
                _opengl_widget->height());
        vs.preserve_aspect_ratio = false;
        return vs;
    });
}

void EventPlotWidget::wireVerticalAxis() {
    if (!_vertical_axis_widget || !_state) {
        return;
    }

    // Dynamic axis mapping — updates when trial count changes
    connect(_opengl_widget, &EventPlotOpenGLWidget::trialCountChanged,
            this, [this](size_t count) {
                if (count > 0) {
                    _vertical_axis_widget->setAxisMapping(
                            CorePlotting::trialIndexAxis(count));
                }
            });

    if (_trial_count > 0) {
        _vertical_axis_widget->setAxisMapping(
                CorePlotting::trialIndexAxis(_trial_count));
    }

    // RangeGetter for axis tick rendering — delegates to shared computation
    _vertical_axis_widget->setRangeGetter(
            [this]() { return computeVisibleTrialRange(); });

    // Bidirectional sync Flow A: AxisState (spinboxes) → ViewState zoom/pan
    if (_vertical_axis_state) {
        connect(_vertical_axis_state.get(), &VerticalAxisState::rangeChanged,
                this, [this](double min_range, double max_range) {
                    if (!_state || _trial_count == 0) {
                        return;
                    }
                    auto const mapping = CorePlotting::trialIndexAxis(_trial_count);
                    double const world_y_min = mapping.domainToWorld(min_range);
                    double const world_y_max = mapping.domainToWorld(max_range);
                    double const world_range = world_y_max - world_y_min;
                    if (world_range > 0.001) {
                        _state->setYZoom(2.0 / world_range);
                        _state->setPan(_state->viewState().x_pan,
                                       (world_y_min + world_y_max) / 2.0);
                    }
                });
    }
}

void EventPlotWidget::connectViewChangeSignals() {
    // Single handler consolidates all view-change reactions:
    //   • repaint both axis widgets
    //   • sync time & vertical range controls (silent — avoids feedback loops)
    auto onViewChanged = [this]() {
        if (_axis_widget) {
            _axis_widget->update();
        }
        if (_vertical_axis_widget) {
            _vertical_axis_widget->update();
        }
        syncTimeAxisRange();
        syncVerticalAxisRange();
    };

    // ViewState changes come from EventPlotState (zoom/pan/bounds setters)
    connect(_state.get(), &EventPlotState::viewStateChanged,
            this, onViewChanged);

    // viewBoundsChanged comes from the OpenGL widget (resize, interaction)
    connect(_opengl_widget, &EventPlotOpenGLWidget::viewBoundsChanged,
            this, onViewChanged);
}

void EventPlotWidget::syncTimeAxisRange() {
    auto * time_axis_state = _state ? _state->relativeTimeAxisState() : nullptr;
    if (!time_axis_state) {
        return;
    }
    auto [min, max] = computeVisibleTimeRange();
    time_axis_state->setRangeSilent(min, max);
}

void EventPlotWidget::syncVerticalAxisRange() {
    if (!_vertical_axis_state) {
        return;
    }
    auto [min, max] = computeVisibleTrialRange();
    _vertical_axis_state->setRangeSilent(min, max);
}

// ---------------------------------------------------------------------------
// Visible-range helpers — single source of truth for zoom/pan → domain math
// ---------------------------------------------------------------------------

std::pair<double, double> EventPlotWidget::computeVisibleTrialRange() const {
    if (_trial_count == 0 || !_state) {
        return {0.0, 0.0};
    }
    auto const & vs = _state->viewState();
    auto const mapping = CorePlotting::trialIndexAxis(_trial_count);

    // Y world coordinates span [-1, 1]; zoom/pan shift the visible window
    double const zoomed_half = 1.0 / vs.y_zoom;
    double const visible_bottom = -zoomed_half + vs.y_pan;
    double const visible_top = zoomed_half + vs.y_pan;

    double const a = mapping.worldToDomain(visible_bottom);
    double const b = mapping.worldToDomain(visible_top);
    return {std::min(a, b), std::max(a, b)};
}

std::pair<double, double> EventPlotWidget::computeVisibleTimeRange() const {
    if (!_state) {
        return {0.0, 0.0};
    }
    auto const & vs = _state->viewState();
    double const half = (vs.x_max - vs.x_min) / 2.0 / vs.x_zoom;
    double const center = (vs.x_min + vs.x_max) / 2.0;
    return {center - half + vs.x_pan, center + half + vs.x_pan};
}

void EventPlotWidget::resizeEvent(QResizeEvent * event) {
    QWidget::resizeEvent(event);
    // Update axis widget when widget resizes to ensure it gets fresh viewport dimensions
    if (_axis_widget) {
        _axis_widget->update();
    }
}

EventPlotState * EventPlotWidget::state() {
    return _state.get();
}

RelativeTimeAxisRangeControls * EventPlotWidget::getRangeControls() const {
    return _range_controls;
}

void EventPlotWidget::handleExportSVG() {
    QString const fileName = AppFileDialog::getSaveFileName(
            this,
            QStringLiteral("export_event_plot_svg"),
            tr("Export Event Plot to SVG"),
            tr("SVG Files (*.svg);;All Files (*)"));
    if (fileName.isEmpty()) {
        return;
    }

    QString const svg = _opengl_widget->exportToSVG();
    if (svg.isEmpty()) {
        QMessageBox::warning(
                this,
                tr("Export Failed"),
                tr("No scene to export. Load data and configure the plot first."));
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(
                this,
                tr("Export Failed"),
                tr("Could not open file for writing:\n%1").arg(fileName));
        return;
    }

    QTextStream out(&file);
    out << svg;
    file.close();

    QMessageBox::information(
            this,
            tr("Export Successful"),
            tr("Event plot exported to:\n%1").arg(fileName));
}

void EventPlotWidget::handleExportCSV() {
    QString const fileName = AppFileDialog::getSaveFileName(
            this,
            QStringLiteral("export_event_plot_csv"),
            tr("Export Event Plot to CSV"),
            tr("CSV Files (*.csv);;All Files (*)"));
    if (fileName.isEmpty()) {
        return;
    }

    auto bundle = _opengl_widget->collectRasterExportData();
    if (bundle.inputs.empty()) {
        QMessageBox::warning(
                this,
                tr("Export Failed"),
                tr("No data to export. Load data and configure the plot first."));
        return;
    }

    PlotDataExport::RasterExportMetadata metadata;
    if (_state) {
        metadata.alignment_key = _state->getAlignmentEventKey().toStdString();
        metadata.window_size = _state->getWindowSize();
    }

    std::string const csv = PlotDataExport::exportRasterToCSV(bundle.inputs, metadata);

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(
                this,
                tr("Export Failed"),
                tr("Could not open file for writing:\n%1").arg(fileName));
        return;
    }

    QTextStream out(&file);
    out << QString::fromStdString(csv);
    file.close();

    QMessageBox::information(
            this,
            tr("Export Successful"),
            tr("Event plot CSV exported to:\n%1").arg(fileName));
}


VerticalAxisRangeControls * EventPlotWidget::getVerticalRangeControls() const {
    return _vertical_range_controls;
}

VerticalAxisState * EventPlotWidget::getVerticalAxisState() const {
    return _vertical_axis_state.get();
}

void EventPlotWidget::_registerDataCallback(std::string const & key) {
    if (!_data_manager || key.empty()) {
        return;
    }
    if (_data_callback_ids.find(key) != _data_callback_ids.end()) {
        return;// Already registered
    }
    int const id = _data_manager->addCallbackToData(key, [this]() {
        if (_state) {
            emit _state->stateChanged();
        }
    });
    if (id >= 0) {
        _data_callback_ids[key] = id;
    }
}

void EventPlotWidget::_unregisterDataCallback(std::string const & key) {
    auto it = _data_callback_ids.find(key);
    if (it == _data_callback_ids.end()) {
        return;
    }
    if (_data_manager) {
        _data_manager->removeCallbackFromData(key, it->second);
    }
    _data_callback_ids.erase(it);
}

void EventPlotWidget::_unregisterAllDataCallbacks() {
    if (!_data_manager) {
        return;
    }
    for (auto const & [key, id]: _data_callback_ids) {
        _data_manager->removeCallbackFromData(key, id);
    }
    _data_callback_ids.clear();
}

void EventPlotWidget::_syncDataCallbacks() {
    if (!_state) {
        return;
    }

    // Collect the set of DM keys currently in state (plot events + alignment)
    std::set<std::string> current_keys;
    for (auto const & name: _state->getPlotEventNames()) {
        auto opts = _state->getPlotEventOptions(name);
        if (opts) {
            current_keys.insert(opts->event_key);
        }
    }
    auto const alignment_key = _state->getAlignmentEventKey().toStdString();
    if (!alignment_key.empty()) {
        current_keys.insert(alignment_key);
    }

    // Remove callbacks for keys no longer in state
    std::vector<std::string> to_remove;
    for (auto const & [key, id]: _data_callback_ids) {
        if (current_keys.find(key) == current_keys.end()) {
            to_remove.push_back(key);
        }
    }
    for (auto const & key: to_remove) {
        _unregisterDataCallback(key);
    }

    // Add callbacks for new keys
    for (auto const & key: current_keys) {
        _registerDataCallback(key);
    }
}

void EventPlotWidget::_pruneRemovedKeys() {
    if (!_state || !_data_manager) {
        return;
    }
    auto const all_keys = _data_manager->getAllKeys();
    std::set<std::string> const key_set(all_keys.begin(), all_keys.end());

    // Find plot events whose DM keys no longer exist
    std::vector<QString> to_remove;
    for (auto const & name: _state->getPlotEventNames()) {
        auto opts = _state->getPlotEventOptions(name);
        if (opts && key_set.find(opts->event_key) == key_set.end()) {
            _unregisterDataCallback(opts->event_key);
            to_remove.push_back(name);
        }
    }
    for (auto const & name: to_remove) {
        _state->removePlotEvent(name);
    }

    // Prune alignment key if removed
    auto const alignment_key = _state->getAlignmentEventKey().toStdString();
    if (!alignment_key.empty() && key_set.find(alignment_key) == key_set.end()) {
        _unregisterDataCallback(alignment_key);
        _state->setAlignmentEventKey(QString());
    }
}
