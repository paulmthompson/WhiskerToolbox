#include "TimeScrollBar.hpp"

#include "ui_TimeScrollBar.h"

#include "DataManager/DataManager.hpp"
#include "DataObjects/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "FrameFilter.hpp"
#include "KeymapSystem/KeyActionAdapter.hpp"
#include "KeymapSystem/KeymapManager.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"// For TimeKey
#include "TimeFrame/TimeFrame.hpp"
#include "TimeScrollBarPlayback.hpp"
#include "TimeScrollBarState.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QTimer>

#include <cmath>
#include <iostream>
#include <utility>

namespace {

[[nodiscard]] QString formatFpsLabel(float fps) noexcept {
    float const r = std::round(fps);
    if (std::abs(fps - r) < 1e-3F) {
        return QString::number(static_cast<int>(r));
    }
    return QString::number(static_cast<double>(fps), 'f', 1);
}

}// namespace

/**
 * @brief Helper function to setup common UI connections
 */
void TimeScrollBar::_setupConnections() {
    connect(_timer, &QTimer::timeout, this, &TimeScrollBar::_vidLoop);

    connect(ui->horizontalScrollBar, &QScrollBar::valueChanged, this, &TimeScrollBar::Slider_Scroll);
    connect(ui->horizontalScrollBar, &QScrollBar::sliderMoved, this, &TimeScrollBar::Slider_Drag);

    connect(ui->play_button, &QPushButton::clicked, this, &TimeScrollBar::PlayButton);
    connect(ui->rewind, &QPushButton::clicked, this, &TimeScrollBar::RewindButton);
    connect(ui->fastforward, &QPushButton::clicked, this, &TimeScrollBar::FastForwardButton);

    // Set up spin box with keyboard tracking disabled (only update on Enter key or arrow clicks)
    ui->frame_spinbox->setKeyboardTracking(false);
    connect(ui->frame_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), this, &TimeScrollBar::FrameSpinBoxChanged);

    // Connect frame jump spinbox to state if available
    connect(ui->frame_jump_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        if (_state) {
            _state->setFrameJump(value);
        }
    });

    // Connect TimeKey selector if it exists
    if (ui->timekey_combobox) {
        connect(ui->timekey_combobox, QOverload<QString const &>::of(&QComboBox::currentTextChanged),
                this, &TimeScrollBar::_onTimeKeyChanged);
    }

    // Connect frame-filter controls if present
    if (ui->skip_tracked_checkbox) {
        connect(ui->skip_tracked_checkbox, &QCheckBox::toggled, this, [this](bool /*checked*/) {
            _rebuildFrameFilter();
        });
    }
    if (ui->filter_key_combobox) {
        connect(ui->filter_key_combobox,
                QOverload<QString const &>::of(&QComboBox::currentTextChanged),
                this, [this](QString const & /*key*/) {
                    _rebuildFrameFilter();
                });
    }
}

/**
 * @brief Initialize UI from state values
 */
void TimeScrollBar::_initializeFromState() {
    if (!_state) return;

    // Block signals during initialization to avoid feedback loops
    ui->frame_jump_spinbox->blockSignals(true);

    // Set UI values from state
    _target_fps = _state->targetFps();
    ui->frame_jump_spinbox->setValue(_state->frameJump());

    ui->fps_label->setText(formatFpsLabel(_target_fps));

    // Unblock signals
    ui->frame_jump_spinbox->blockSignals(false);
}

/**
 * @brief TimeScrollBar constructor with EditorState support
 */
TimeScrollBar::TimeScrollBar(std::shared_ptr<DataManager> data_manager,
                             std::shared_ptr<TimeScrollBarState> state,
                             QWidget * parent)
    : QWidget(parent),
      ui(new Ui::TimeScrollBar),
      _data_manager{std::move(data_manager)},
      _state{std::move(state)},
      _data_manager_observer_id{-1} {
    ui->setupUi(this);

    _timer = new QTimer(this);

    _setupConnections();

    // Initialize UI from state if provided
    if (_state) {
        _initializeFromState();
    }

    // Register for DataManager notifications if available
    if (_data_manager) {
        _data_manager_observer_id = _data_manager->addObserver([this]() {
            _onDataManagerChanged();
        });
        _populateTimeKeySelector();
    }
}

/**
 * @brief Legacy TimeScrollBar constructor (backward compatible)
 */
TimeScrollBar::TimeScrollBar(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::TimeScrollBar),
      _state{nullptr},
      _data_manager_observer_id{-1} {
    ui->setupUi(this);

    _timer = new QTimer(this);

    _setupConnections();
};

TimeScrollBar::~TimeScrollBar() {
    // Remove DataManager observer if registered
    if (_data_manager && _data_manager_observer_id >= 0) {
        _data_manager->removeObserver(_data_manager_observer_id);
        _data_manager_observer_id = -1;
    }

    delete ui;
    _timer->stop();
}

/*
We can click and hold the slider to move to a new position
In the case that we are dragging the slider, to make this optimally smooth, we should not add any new decoding frames
until we have finished the most recent one.
 */

void TimeScrollBar::Slider_Drag(int newPos) {
    static_cast<void>(newPos);

    auto current_frame = ui->horizontalScrollBar->sliderPosition();
    auto snap_frame = _getSnapFrame(current_frame);
    ui->horizontalScrollBar->setSliderPosition(snap_frame);
}


void TimeScrollBar::Slider_Scroll(int newPos) {
    if (_verbose) {
        std::cout << "The slider position is " << ui->horizontalScrollBar->sliderPosition() << std::endl;
    }

    std::shared_ptr<TimeFrame> tf = _current_time_frame;

    if (_current_time_frame == nullptr) {

        std::cout << "No TimeFrame available during Slider_Scroll" << std::endl;
        return;
    }

    tf = _current_time_frame;

    auto frame_id = tf->checkFrameInbounds(newPos);
    ui->horizontalScrollBar->setSliderPosition(newPos);

    // Create TimePosition with the actual TimeFrame pointer
    TimePosition const position(TimeFrameIndex(frame_id), tf);

    // Update EditorRegistry directly (preferred path)
    if (_editor_registry) {
        _editor_registry->setCurrentTime(position);
    } else {
        if (_verbose) {
            std::cout << "No EditorRegistry available during Slider_Scroll" << std::endl;
        }
    }

    // Update frame labels
    _updateFrameLabels(frame_id);

    // Emit new signal (preferred)
    emit timeChanged(position);

    // Emit deprecated signal for backward compatibility
    emit timeChanged(frame_id);
}


void TimeScrollBar::_updateFrameLabels(int frame_num) {
    // Use current TimeFrame if available, otherwise fall back to DataManager's default
    std::shared_ptr<TimeFrame> tf = _current_time_frame;
    if (!tf && _data_manager) {
        tf = _data_manager->getTime();
    }

    if (!tf) {
        // No TimeFrame available - can't update labels
        return;
    }

    auto video_time = tf->getTimeAtIndex(TimeFrameIndex(frame_num));

    ui->time_label->setText(QString::number(video_time));

    // Update the spin box value without triggering valueChanged signal
    ui->frame_spinbox->blockSignals(true);
    ui->frame_spinbox->setValue(frame_num);
    ui->frame_spinbox->blockSignals(false);
}

void TimeScrollBar::updateScrollBarNewMax(int new_max) {

    ui->frame_count_label->setText(QString::number(new_max));
    ui->horizontalScrollBar->setMaximum(new_max);
    ui->frame_spinbox->setMaximum(new_max);
}


void TimeScrollBar::PlayButton() {

    if (_play_mode) {

        _timer->stop();
        ui->play_button->setText(QString("Play"));
        _play_mode = false;

        auto current_pos = _editor_registry->currentPosition();
        if (current_pos.isValid() && current_pos.sameClock(_current_time_frame)) {
            ui->horizontalScrollBar->blockSignals(true);
            ui->horizontalScrollBar->setValue(static_cast<int>(current_pos.index.getValue()));
            ui->horizontalScrollBar->blockSignals(false);
        }


    } else {
        ui->play_button->setText(QString("Pause"));
        int const period_ms = time_scroll_bar::timerPeriodMsForTargetFps(_target_fps);
        _timer->setInterval(period_ms);
        _timer->start(period_ms);
        _play_mode = true;
    }

    if (_state) {
        _state->setIsPlaying(_play_mode);
    }
}

void TimeScrollBar::_updatePlaybackTimerInterval() {
    if (!_play_mode) {
        return;
    }
    int const period_ms = time_scroll_bar::timerPeriodMsForTargetFps(_target_fps);
    _timer->setInterval(period_ms);
    if (!_timer->isActive()) {
        _timer->start(period_ms);
    }
}

void TimeScrollBar::RewindButton() {
    std::size_t const idx = time_scroll_bar::presetIndexForTargetFps(_target_fps);
    if (idx == 0) {
        return;
    }
    _target_fps = time_scroll_bar::kFpsPresets[idx - 1];
    ui->fps_label->setText(formatFpsLabel(_target_fps));

    if (_state) {
        _state->setTargetFps(_target_fps);
    }
    _updatePlaybackTimerInterval();
}

void TimeScrollBar::FastForwardButton() {
    std::size_t const idx = time_scroll_bar::presetIndexForTargetFps(_target_fps);
    if (idx + 1 >= time_scroll_bar::kFpsPresets.size()) {
        return;
    }
    _target_fps = time_scroll_bar::kFpsPresets[idx + 1];
    ui->fps_label->setText(formatFpsLabel(_target_fps));

    if (_state) {
        _state->setTargetFps(_target_fps);
    }
    _updatePlaybackTimerInterval();
}

void TimeScrollBar::_vidLoop() {
    // Get current time from EditorRegistry if available, otherwise from DataManager
    int current_time = 0;
    auto current_pos = _editor_registry->currentPosition();
    if (current_pos.isValid() && current_pos.sameClock(_current_time_frame)) {
        current_time = static_cast<int>(current_pos.index.getValue()) + 1;
    }

    // Apply frame filter: scan forward until a non-skipped frame is found.
    if (_frame_filter) {
        int const max_value = ui->horizontalScrollBar->maximum();
        int const candidate = frame_filter::scanToNextNonSkipped(
                *_frame_filter, current_time, /*direction=*/1,
                ui->horizontalScrollBar->minimum(), max_value);
        if (candidate < 0) {
            // All remaining frames are skipped — stop playback.
            _timer->stop();
            ui->play_button->setText(QStringLiteral("Play"));
            _play_mode = false;
            if (_state) {
                _state->setIsPlaying(false);
            }
            return;
        }
        current_time = candidate;
    }

    ui->horizontalScrollBar->setSliderPosition(current_time);
}

void TimeScrollBar::changeScrollBarValue(int new_value, bool relative) {
    int const min_value = ui->horizontalScrollBar->minimum();
    int const max_value = ui->horizontalScrollBar->maximum();

    int direction = 1;// default: forward
    if (relative) {
        // Get current time from EditorRegistry if available, otherwise from DataManager
        int current_time = 0;

        auto current_pos = _editor_registry->currentPosition();
        if (current_pos.isValid() && current_pos.sameClock(_current_time_frame)) {
            current_time = static_cast<int>(current_pos.index.getValue());
        }
        direction = (new_value >= 0) ? 1 : -1;
        new_value = current_time + new_value;
    }

    if (new_value < min_value) {
        std::cout << "Cannot set value below minimum" << std::endl;
        return;
    }

    if (new_value > max_value) {
        std::cout << "Cannot set value above maximum" << std::endl;
        return;
    }

    // Apply frame filter: find the nearest non-skipped frame in the intended direction.
    if (_frame_filter) {
        int const candidate = frame_filter::scanToNextNonSkipped(
                *_frame_filter, new_value, direction, min_value, max_value);
        if (candidate < 0) {
            // No non-skipped frame found — do not move.
            return;
        }
        new_value = candidate;
    }

    Slider_Scroll(new_value);
}

void TimeScrollBar::FrameSpinBoxChanged(int new_frame) {
    // Use current TimeFrame if available, otherwise fall back to DataManager's default
    std::shared_ptr<TimeFrame> tf = _current_time_frame;
    if (!tf && _data_manager) {
        tf = _data_manager->getTime();
    }

    if (!tf) {
        // No TimeFrame available - can't proceed
        return;
    }

    auto frame_id = tf->checkFrameInbounds(new_frame);

    // Create TimePosition with the actual TimeFrame pointer
    TimePosition const position(TimeFrameIndex(frame_id), tf);

    // Update EditorRegistry directly (preferred path)
    if (_editor_registry) {
        _editor_registry->setCurrentTime(position);
    }

    ui->horizontalScrollBar->setSliderPosition(frame_id);
    _updateFrameLabels(frame_id);

    // Emit new signal (preferred)
    emit timeChanged(position);

    // Emit deprecated signal for backward compatibility
    emit timeChanged(frame_id);
}

int TimeScrollBar::getFrameJumpValue() const {
    return ui->frame_jump_spinbox->value();
}

void TimeScrollBar::setEditorRegistry(EditorRegistry * registry) {
    // Disconnect old registry if it exists
    if (_editor_registry) {
        disconnect(_editor_registry, &EditorRegistry::timeChanged,
                   this, &TimeScrollBar::_onEditorRegistryTimeChanged);
    }

    _editor_registry = registry;

    // Connect to new registry
    if (_editor_registry) {
        connect(_editor_registry, &EditorRegistry::timeChanged,
                this, &TimeScrollBar::_onEditorRegistryTimeChanged);
    }
}

void TimeScrollBar::setKeymapManager(KeymapSystem::KeymapManager * manager) {
    if (!manager) {
        return;
    }

    _key_adapter = new KeymapSystem::KeyActionAdapter(this);
    _key_adapter->setTypeId(EditorLib::EditorTypeId(QStringLiteral("TimeScrollBar")));

    _key_adapter->setHandler([this](QString const & action_id) -> bool {
        if (action_id == QStringLiteral("timeline.play_pause")) {
            PlayButton();
            return true;
        }
        if (action_id == QStringLiteral("timeline.next_frame")) {
            changeScrollBarValue(1, true);
            return true;
        }
        if (action_id == QStringLiteral("timeline.prev_frame")) {
            changeScrollBarValue(-1, true);
            return true;
        }
        if (action_id == QStringLiteral("timeline.jump_forward")) {
            changeScrollBarValue(getFrameJumpValue(), true);
            return true;
        }
        if (action_id == QStringLiteral("timeline.jump_backward")) {
            changeScrollBarValue(-getFrameJumpValue(), true);
            return true;
        }
        return false;
    });

    manager->registerAdapter(_key_adapter);
}

void TimeScrollBar::setTimeFrame(std::shared_ptr<TimeFrame> const & tf, TimeKey display_key) {
    _current_time_frame = tf;
    _current_display_key = std::move(display_key);

    if (tf) {
        updateScrollBarNewMax(static_cast<int>(tf->getTotalFrameCount() - 1));
    }

    // Propagate the new TimeFrame to the active filter so coordinate conversion
    // remains correct after a timeframe switch.
    if (_frame_filter) {
        if (auto * dkf = dynamic_cast<DataKeyFrameFilter *>(_frame_filter.get())) {
            dkf->setTimeFrame(_current_time_frame);
        }
    }

    // Reset to frame 0
    changeScrollBarValue(0);
}

void TimeScrollBar::setFrameFilter(std::unique_ptr<FrameFilter> filter) {
    _frame_filter = std::move(filter);
    // Ensure the filter knows the current TimeFrame.
    if (_frame_filter) {
        if (auto * dkf = dynamic_cast<DataKeyFrameFilter *>(_frame_filter.get())) {
            dkf->setTimeFrame(_current_time_frame);
        }
    }
}

void TimeScrollBar::_populateTimeKeySelector() {
    if (!_data_manager || !ui->timekey_combobox) {
        return;
    }

    ui->timekey_combobox->blockSignals(true);
    ui->timekey_combobox->clear();

    for (auto const & key: _data_manager->getTimeFrameKeys()) {
        ui->timekey_combobox->addItem(QString::fromStdString(key.str()));
    }

    // Set current selection if available
    QString const current_key_str = QString::fromStdString(_current_display_key.str());
    int const index = ui->timekey_combobox->findText(current_key_str);
    if (index >= 0) {
        ui->timekey_combobox->setCurrentIndex(index);
    }

    ui->timekey_combobox->blockSignals(false);
}

void TimeScrollBar::_onTimeKeyChanged(QString const & key_str) {
    if (!_data_manager) {
        return;
    }

    TimeKey const key(key_str.toStdString());
    auto tf = _data_manager->getTime(key);
    if (tf) {
        setTimeFrame(tf, key);
    }
}

void TimeScrollBar::_populateFilterKeySelector() {
    if (!_data_manager || !ui->filter_key_combobox) {
        return;
    }

    // Preserve currently selected key so we can restore it after repopulating.
    QString const previous = ui->filter_key_combobox->currentText();

    ui->filter_key_combobox->blockSignals(true);
    ui->filter_key_combobox->clear();

    for (auto const & key: _data_manager->getKeys<DigitalIntervalSeries>()) {
        ui->filter_key_combobox->addItem(QString::fromStdString(key));
    }

    // Restore the previous selection if it still exists.
    int const idx = ui->filter_key_combobox->findText(previous);
    if (idx >= 0) {
        ui->filter_key_combobox->setCurrentIndex(idx);
    }

    ui->filter_key_combobox->blockSignals(false);
}

void TimeScrollBar::_rebuildFrameFilter() {
    if (!ui->skip_tracked_checkbox || !ui->filter_key_combobox) {
        _frame_filter = nullptr;
        return;
    }

    if (!ui->skip_tracked_checkbox->isChecked()) {
        _frame_filter = nullptr;
        return;
    }

    QString const key_str = ui->filter_key_combobox->currentText();
    if (key_str.isEmpty() || !_data_manager) {
        _frame_filter = nullptr;
        return;
    }

    auto filter = std::make_unique<DataKeyFrameFilter>(
            _data_manager, key_str.toStdString(), /*skip_tracked=*/true);
    filter->setTimeFrame(_current_time_frame);
    _frame_filter = std::move(filter);
}

void TimeScrollBar::_onEditorRegistryTimeChanged(TimePosition const & position) {
    // Only update if the time change is for the same clock we're controlling
    if (!position.isValid()) {
        std::cout << "TimeScrollBar::_onEditorRegistryTimeChanged: Invalid time position" << std::endl;
        return;
    }
    if (!_current_time_frame) {
        std::cout << "TimeScrollBar::_onEditorRegistryTimeChanged: No current time frame" << std::endl;
        return;
    }

    int const frame_value = position.convertTo(_current_time_frame.get()).getValue();
    ui->horizontalScrollBar->blockSignals(true);
    ui->horizontalScrollBar->setSliderPosition(frame_value);
    ui->horizontalScrollBar->blockSignals(false);

    // Update labels
    _updateFrameLabels(frame_value);
}

void TimeScrollBar::setDataManager(std::shared_ptr<DataManager> data_manager) {
    // Remove observer from old DataManager if it exists
    if (_data_manager && _data_manager_observer_id >= 0) {
        _data_manager->removeObserver(_data_manager_observer_id);
        _data_manager_observer_id = -1;
    }

    _data_manager = std::move(data_manager);

    // Register observer with new DataManager if available
    if (_data_manager) {
        _data_manager_observer_id = _data_manager->addObserver([this]() {
            _onDataManagerChanged();
        });
        _populateTimeKeySelector();
    }
}

void TimeScrollBar::_onDataManagerChanged() {
    if (!_data_manager) {
        std::cout << "No DataManager available during TimeScrollBar::_onDataManagerChanged" << std::endl;
        return;
    }

    // Try to reget the timeframe for the existing key
    std::shared_ptr<TimeFrame> tf = nullptr;
    if (!_current_display_key.str().empty()) {
        tf = _data_manager->getTime(_current_display_key);
    }

    // If that doesn't work, get the default timeframe
    if (!tf) {
        tf = _data_manager->getTime();
        if (tf) {
            _current_display_key = TimeKey("time");
        }
    }

    // Update the current timeframe if we found one
    if (tf) {
        _current_time_frame = tf;

        // Update scrollbar max if timeframe changed
        updateScrollBarNewMax(static_cast<int>(tf->getTotalFrameCount() - 1));

        // Repopulate TimeKey selector to reflect current state
        _populateTimeKeySelector();

        // Repopulate filter key selector (new DigitalIntervalSeries may have been added)
        _populateFilterKeySelector();
    } else {
        // No timeframe available - clear it
        _current_time_frame = nullptr;
        std::cout << "No timeframe available during TimeScrollBar::_onDataManagerChanged" << std::endl;
    }
}
