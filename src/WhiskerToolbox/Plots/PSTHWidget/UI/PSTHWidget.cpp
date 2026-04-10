#include "PSTHWidget.hpp"

#include "Core/PSTHState.hpp"
#include "CorePlotting/CoordinateTransform/AxisMapping.hpp"
#include "CorePlotting/CoordinateTransform/ViewState.hpp"
#include "DataManager/DataManager.hpp"
#include "PlotDataExport/HistogramCSVExport.hpp"
#include "Plots/Common/EventRateEstimation/EstimationParams.hpp"
#include "Plots/Common/EventRateEstimation/RateEstimate.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWithRangeControls.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisState.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWidget.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"
#include "Rendering/PSTHPlotOpenGLWidget.hpp"
#include "StateManagement/AppFileDialog.hpp"

#include <QFile>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QResizeEvent>
#include <QTextStream>
#include <QVBoxLayout>
#include <set>
#include <utility>

#include "ui_PSTHWidget.h"

PSTHWidget::PSTHWidget(std::shared_ptr<DataManager> data_manager,
                       QWidget * parent)
    : QWidget(parent),
      _data_manager(std::move(std::move(data_manager))),
      ui(new Ui::PSTHWidget),
      _opengl_widget(nullptr),
      _axis_widget(nullptr),
      _range_controls(nullptr),
      _vertical_axis_widget(nullptr),
      _vertical_range_controls(nullptr) {
    ui->setupUi(this);

    // Create horizontal layout for vertical axis + OpenGL widget
    auto * horizontal_layout = new QHBoxLayout();
    horizontal_layout->setSpacing(0);
    horizontal_layout->setContentsMargins(0, 0, 0, 0);

    // Vertical axis widget and controls will be created in setState()
    // when we have access to the PSTHState's VerticalAxisState
    // For now, add a placeholder (will be replaced in setState)
    _vertical_axis_widget = nullptr;
    _vertical_range_controls = nullptr;

    // Create and add the OpenGL widget
    _opengl_widget = new PSTHPlotOpenGLWidget(this);
    _opengl_widget->setDataManager(_data_manager);
    horizontal_layout->addWidget(_opengl_widget, 1);// Stretch factor 1

    // Create vertical layout for horizontal layout + time axis
    auto * vertical_layout = new QVBoxLayout();
    vertical_layout->setSpacing(0);
    vertical_layout->setContentsMargins(0, 0, 0, 0);
    vertical_layout->addLayout(horizontal_layout, 1);// Stretch factor 1

    // Time axis widget and controls will be created in setState()
    // when we have access to the PSTHState's RelativeTimeAxisState
    _axis_widget = nullptr;
    _range_controls = nullptr;

    // Replace the main layout
    QLayout * old_layout = layout();

    delete old_layout;

    setLayout(vertical_layout);

    // Forward signals from OpenGL widget
    connect(_opengl_widget, &PSTHPlotOpenGLWidget::plotDoubleClicked,
            this, [this](int64_t time_frame_index) {
                emit timePositionSelected(TimePosition(time_frame_index));
            });
}

PSTHWidget::~PSTHWidget() {
    _unregisterAllDataCallbacks();
    if (_data_manager && _dm_observer_id != -1) {
        _data_manager->removeObserver(_dm_observer_id);
    }
    delete ui;
}

void PSTHWidget::setState(std::shared_ptr<PSTHState> state) {
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
    connect(_state.get(), &PSTHState::plotEventAdded,
            this, [this](QString const & /* event_name */) {
                _syncDataCallbacks();
            });
    connect(_state.get(), &PSTHState::plotEventRemoved,
            this, [this](QString const & /* event_name */) {
                _syncDataCallbacks();
            });

    // Track alignment key changes to swap callback
    connect(_state.get(), &PSTHState::alignmentEventKeyChanged,
            this, [this](QString const & /* key */) {
                _syncDataCallbacks();
            });

    // Register DataManager-level observer to detect key removal
    if (_data_manager && _dm_observer_id == -1) {
        _dm_observer_id = _data_manager->addObserver([this]() {
            _pruneRemovedKeys();
        });
    }

    syncTimeAxisRange();
    syncVerticalAxisRange();
}

// ---------------------------------------------------------------------------
// setState decomposition
// ---------------------------------------------------------------------------

void PSTHWidget::createTimeAxisIfNeeded() {
    if (_axis_widget) {
        return;
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

void PSTHWidget::wireTimeAxis() {
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

void PSTHWidget::wireVerticalAxis() {
    // Create the vertical axis widget if it doesn't exist yet
    if (!_vertical_axis_widget && _state) {
        auto * vertical_axis_state = _state->verticalAxisState();
        if (vertical_axis_state) {
            auto result = createVerticalAxisWithRangeControls(
                    vertical_axis_state, this, nullptr);
            _vertical_axis_widget = result.axis_widget;
            _vertical_range_controls = result.range_controls;

            // Insert into the horizontal layout (before the OpenGL widget)
            if (_vertical_axis_widget) {
                if (auto * vbox = qobject_cast<QVBoxLayout *>(layout())) {
                    if (vbox->count() > 0) {
                        auto * item = vbox->itemAt(0);
                        if (item && item->layout()) {
                            if (auto * hbox = qobject_cast<QHBoxLayout *>(item->layout())) {
                                hbox->insertWidget(0, _vertical_axis_widget);
                            }
                        }
                    }
                }
            }
        }
    }

    if (!_vertical_axis_widget || !_state) {
        return;
    }

    // The vertical axis is a simple linear scale (count axis).
    // The RangeGetter is already set by createVerticalAxisWithRangeControls
    // from the VerticalAxisState, which handles the y_min/y_max directly.
    // We use identity axis mapping (domain == world) for labels.
    _vertical_axis_widget->setAxisMapping(CorePlotting::identityAxis("Count", 0));

    // Bidirectional sync: VerticalAxisState → ViewState y_zoom/y_pan
    auto * vas = _state->verticalAxisState();
    if (vas) {
        connect(vas, &VerticalAxisState::rangeChanged,
                this, [this](double min_range, double max_range) {
                    if (!_state) return;
                    double const range = max_range - min_range;
                    if (range > 0.001) {
                        auto * vas_local = _state->verticalAxisState();
                        double const full_range = vas_local->getYMax() - vas_local->getYMin();
                        // Zoom = full_range / visible_range
                        _state->setYZoom(full_range / range);
                        _state->setPan(_state->viewState().x_pan,
                                       ((min_range + max_range) / 2.0) -
                                               ((vas_local->getYMin() + vas_local->getYMax()) / 2.0));
                    }
                });
    }
}

void PSTHWidget::connectViewChangeSignals() {
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

    connect(_state.get(), &PSTHState::viewStateChanged,
            this, onViewChanged);

    connect(_opengl_widget, &PSTHPlotOpenGLWidget::viewBoundsChanged,
            this, onViewChanged);
}

void PSTHWidget::syncTimeAxisRange() {
    auto * time_axis_state = _state ? _state->relativeTimeAxisState() : nullptr;
    if (!time_axis_state) {
        return;
    }
    auto [min, max] = computeVisibleTimeRange();
    time_axis_state->setRangeSilent(min, max);
}

void PSTHWidget::syncVerticalAxisRange() {
    if (!_state) {
        return;
    }
    auto * vas = _state->verticalAxisState();
    if (!vas) {
        return;
    }
    auto [min, max] = computeVisibleVerticalRange();
    vas->setRangeSilent(min, max);
}

// ---------------------------------------------------------------------------
// Visible-range helpers
// ---------------------------------------------------------------------------

std::pair<double, double> PSTHWidget::computeVisibleTimeRange() const {
    if (!_state) {
        return {0.0, 0.0};
    }
    auto const & vs = _state->viewState();
    double const half = (vs.x_max - vs.x_min) / 2.0 / vs.x_zoom;
    double const center = (vs.x_min + vs.x_max) / 2.0;
    return {center - half + vs.x_pan, center + half + vs.x_pan};
}

std::pair<double, double> PSTHWidget::computeVisibleVerticalRange() const {
    if (!_state) {
        return {0.0, 100.0};
    }
    // Y data bounds are in ViewStateData (kept in sync with vertical axis via setYBounds)
    auto const & vs = _state->viewState();
    double const y_range = vs.y_max - vs.y_min;
    double const y_center = (vs.y_min + vs.y_max) / 2.0;
    double const half = y_range / 2.0 / vs.y_zoom;
    return {y_center - half + vs.y_pan, y_center + half + vs.y_pan};
}

PSTHState * PSTHWidget::state() {
    return _state.get();
}

RelativeTimeAxisRangeControls * PSTHWidget::getRangeControls() const {
    return _range_controls;
}

VerticalAxisRangeControls * PSTHWidget::getVerticalRangeControls() const {
    return _vertical_range_controls;
}

void PSTHWidget::handleExportSVG() {
    QString const fileName = AppFileDialog::getSaveFileName(
            this,
            QStringLiteral("export_psth_svg"),
            tr("Export PSTH to SVG"),
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
            tr("PSTH exported to:\n%1").arg(fileName));
}

void PSTHWidget::handleExportCSV() {
    QString const fileName = AppFileDialog::getSaveFileName(
            this,
            QStringLiteral("export_psth_csv"),
            tr("Export PSTH to CSV"),
            tr("CSV Files (*.csv);;All Files (*)"));
    if (fileName.isEmpty()) {
        return;
    }

    auto bundle = _opengl_widget->collectHistogramExportData();
    if (bundle.inputs.empty()) {
        QMessageBox::warning(
                this,
                tr("Export Failed"),
                tr("No data to export. Load data and configure the plot first."));
        return;
    }

    PlotDataExport::HistogramExportMetadata metadata;
    if (_state) {
        metadata.alignment_key = _state->getAlignmentEventKey().toStdString();
        metadata.window_size = _state->getWindowSize();
        metadata.scaling_mode = scalingLabel(_state->scaling());

        auto const & params = _state->estimationParams();
        std::visit([&metadata](auto const & p) {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, WhiskerToolbox::Plots::BinningParams>) {
                metadata.estimation_method = "Binning";
            } else if constexpr (std::is_same_v<T, WhiskerToolbox::Plots::GaussianKernelParams>) {
                metadata.estimation_method = "GaussianKernel";
            } else if constexpr (std::is_same_v<T, WhiskerToolbox::Plots::CausalExponentialParams>) {
                metadata.estimation_method = "CausalExponential";
            }
        },
                   params);
    }

    std::string const csv = PlotDataExport::exportHistogramToCSV(bundle.inputs, metadata);

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
            tr("PSTH CSV exported to:\n%1").arg(fileName));
}

void PSTHWidget::resizeEvent(QResizeEvent * event) {
    QWidget::resizeEvent(event);
    if (_axis_widget) {
        _axis_widget->update();
    }
    if (_vertical_axis_widget) {
        _vertical_axis_widget->update();
    }
}

void PSTHWidget::_registerDataCallback(std::string const & key) {
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

void PSTHWidget::_unregisterDataCallback(std::string const & key) {
    auto it = _data_callback_ids.find(key);
    if (it == _data_callback_ids.end()) {
        return;
    }
    if (_data_manager) {
        _data_manager->removeCallbackFromData(key, it->second);
    }
    _data_callback_ids.erase(it);
}

void PSTHWidget::_unregisterAllDataCallbacks() {
    if (!_data_manager) {
        return;
    }
    for (auto const & [key, id]: _data_callback_ids) {
        _data_manager->removeCallbackFromData(key, id);
    }
    _data_callback_ids.clear();
}

void PSTHWidget::_syncDataCallbacks() {
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

void PSTHWidget::_pruneRemovedKeys() {
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
