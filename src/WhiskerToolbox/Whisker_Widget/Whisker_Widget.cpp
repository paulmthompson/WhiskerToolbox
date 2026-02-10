#include "Whisker_Widget.hpp"

#include "ui_Whisker_Widget.h"

#include "DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/transforms/Media/whisker_tracing.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "WhiskerWidgetState.hpp"
#include "janelia_config.hpp"
#include "mainwindow.hpp"
#include "whiskertracker.hpp"

#include "qevent.h"
#include <QElapsedTimer>
#include <QFileDialog>

#include "DeepLearning/models/scm/scm.hpp"

#include <algorithm>
#include <iostream>
#include <string>

Line2D & convert_to_Line2D(whisker::Line2D & whisker_line) {
    return reinterpret_cast<Line2D &>(whisker_line);
}

// Function to convert Line2D to whisker::Line2D without copying
whisker::Line2D & convert_to_whisker_Line2D(Line2D & line) {
    return reinterpret_cast<whisker::Line2D &>(line);
}

std::vector<whisker::Line2D> & convert_to_whisker_Line2D(std::vector<Line2D> & lines) {
    return reinterpret_cast<std::vector<whisker::Line2D> &>(lines);
}

/**
 * @brief Helper function to setup common UI connections
 */
void Whisker_Widget::_setupConnections() {
    connect(ui->trace_button, &QPushButton::clicked, this, &Whisker_Widget::_traceButton);
    connect(ui->dl_trace_button, &QPushButton::clicked, this, &Whisker_Widget::_dlTraceButton);
    connect(ui->dl_add_memory_button, &QPushButton::clicked, this, &Whisker_Widget::_dlAddMemoryButton);
    connect(ui->actionJanelia_Settings, &QAction::triggered, this, &Whisker_Widget::_openJaneliaConfig);
    connect(ui->face_orientation, &QComboBox::currentIndexChanged, this, &Whisker_Widget::_selectFaceOrientation);
    connect(ui->length_threshold_spinbox, &QDoubleSpinBox::valueChanged, this,
            &Whisker_Widget::_changeWhiskerLengthThreshold);
    connect(ui->whisker_pad_combo, &QComboBox::currentTextChanged, this, &Whisker_Widget::_onWhiskerPadComboChanged);
    connect(ui->whisker_pad_frame_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), this, &Whisker_Widget::_onWhiskerPadFrameChanged);
    connect(ui->whisker_number, &QSpinBox::valueChanged, this, &Whisker_Widget::_selectNumWhiskersToTrack);

    connect(ui->manual_whisker_select_spinbox, &QSpinBox::valueChanged, this, &Whisker_Widget::_selectWhisker);

    connect(ui->actionLoad_Janelia_Whiskers, &QAction::triggered, this, &Whisker_Widget::_loadJaneliaWhiskers);

    connect(ui->whisker_clip, &QSpinBox::valueChanged, this, &Whisker_Widget::_changeWhiskerClip);

    connect(ui->auto_dl_checkbox, &QCheckBox::stateChanged, this, [this]() {
        _auto_dl = ui->auto_dl_checkbox->isChecked();
        if (_state) {
            _state->setAutoDL(_auto_dl);
        }
    });

    connect(ui->linking_tol_spinbox, &QSpinBox::valueChanged, this, [this](int val) {
        _linking_tolerance = static_cast<float>(val);
        if (_state) {
            _state->setLinkingTolerance(_linking_tolerance);
        }
    });

    // Mask UI wiring
    connect(ui->use_mask_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
        _use_mask_mode = checked;
        ui->mask_key_combo->setEnabled(checked);
        _populateMaskCombo();
        // Disable Trace button if mask mode is enabled but no masks exist
        bool const has_masks = !_data_manager->getKeys<MaskData>().empty();
        ui->trace_button->setEnabled(!checked || has_masks);
        if (_state) {
            _state->setUseMaskMode(checked);
        }
    });
    connect(ui->mask_key_combo, &QComboBox::currentTextChanged, this, [this](QString const & txt) {
        _selected_mask_key = txt.toStdString();
        if (_state) {
            _state->setSelectedMaskKey(_selected_mask_key);
        }
    });
}

/**
 * @brief Initialize UI from state values
 */
void Whisker_Widget::_initializeFromState() {
    if (!_state) return;

    // Block signals during initialization to avoid feedback loops
    ui->face_orientation->blockSignals(true);
    ui->whisker_number->blockSignals(true);
    ui->length_threshold_spinbox->blockSignals(true);
    ui->whisker_clip->blockSignals(true);
    ui->linking_tol_spinbox->blockSignals(true);
    ui->use_mask_checkbox->blockSignals(true);
    ui->auto_dl_checkbox->blockSignals(true);
    ui->manual_whisker_select_spinbox->blockSignals(true);

    // Set UI values from state
    ui->face_orientation->setCurrentIndex(static_cast<int>(_state->faceOrientation()));
    ui->whisker_number->setValue(_state->numWhiskersToTrack());
    ui->length_threshold_spinbox->setValue(_state->lengthThreshold());
    ui->whisker_clip->setValue(_state->clipLength());
    ui->linking_tol_spinbox->setValue(static_cast<int>(_state->linkingTolerance()));
    ui->use_mask_checkbox->setChecked(_state->useMaskMode());
    ui->auto_dl_checkbox->setChecked(_state->autoDL());
    ui->manual_whisker_select_spinbox->setValue(_state->currentWhisker());

    // Update internal state variables
    _face_orientation = static_cast<Face_Orientation>(static_cast<int>(_state->faceOrientation()));
    _num_whisker_to_track = _state->numWhiskersToTrack();
    _clip_length = _state->clipLength();
    _linking_tolerance = _state->linkingTolerance();
    _use_mask_mode = _state->useMaskMode();
    _auto_dl = _state->autoDL();
    _current_whisker = _state->currentWhisker();
    _current_whisker_pad_key = _state->whiskerPadKey();
    _current_whisker_pad_point = Point2D<float>(_state->whiskerPadX(), _state->whiskerPadY());
    _selected_mask_key = _state->selectedMaskKey();

    // Set whisker tracker parameters
    _wt->setWhiskerLengthThreshold(static_cast<float>(_state->lengthThreshold()));

    // Set head direction based on face orientation
    switch (_state->faceOrientation()) {
        case FaceOrientation::Top:
            _wt->setHeadDirection(0.0f, 1.0f);
            break;
        case FaceOrientation::Bottom:
            _wt->setHeadDirection(0.0f, -1.0f);
            break;
        case FaceOrientation::Left:
            _wt->setHeadDirection(1.0f, 0.0f);
            break;
        case FaceOrientation::Right:
            _wt->setHeadDirection(-1.0f, 0.0f);
            break;
    }

    // Unblock signals
    ui->face_orientation->blockSignals(false);
    ui->whisker_number->blockSignals(false);
    ui->length_threshold_spinbox->blockSignals(false);
    ui->whisker_clip->blockSignals(false);
    ui->linking_tol_spinbox->blockSignals(false);
    ui->use_mask_checkbox->blockSignals(false);
    ui->auto_dl_checkbox->blockSignals(false);
    ui->manual_whisker_select_spinbox->blockSignals(false);
}

/**
 * @brief Whisker_Widget::Whisker_Widget - Constructor with EditorState support
 *
 * @param data_manager DataManager for data access
 * @param state WhiskerWidgetState for configuration persistence
 * @param parent Parent widget
 */
Whisker_Widget::Whisker_Widget(std::shared_ptr<DataManager> data_manager,
                               std::shared_ptr<WhiskerWidgetState> state,
                               QWidget * parent)
    : QMainWindow(parent),
      _wt{std::make_shared<whisker::WhiskerTracker>()},
      _data_manager{std::move(data_manager)},
      _state{std::move(state)},
      ui(new Ui::Whisker_Widget) {
    ui->setupUi(this);

    _data_manager->setData<LineData>("unlabeled_whiskers", TimeKey("time"));

    _janelia_config_widget = new Janelia_Config(_wt);

    dl_model = std::make_unique<dl::SCM>();

    _setupConnections();

    // Initialize UI from state if provided
    if (_state) {
        _initializeFromState();
    }
}

/**
 * @brief Whisker_Widget::Whisker_Widget - Legacy constructor (backward compatible)
 *
 * @param data_manager DataManager for data access
 * @param parent Parent widget
 */
Whisker_Widget::Whisker_Widget(std::shared_ptr<DataManager> data_manager,
                               QWidget * parent)
    : QMainWindow(parent),
      _wt{std::make_shared<whisker::WhiskerTracker>()},
      _data_manager{std::move(data_manager)},
      _state{nullptr},
      ui(new Ui::Whisker_Widget) {
    ui->setupUi(this);

    _data_manager->setData<LineData>("unlabeled_whiskers", TimeKey("time"));

    _janelia_config_widget = new Janelia_Config(_wt);

    dl_model = std::make_unique<dl::SCM>();

    _setupConnections();
};

Whisker_Widget::~Whisker_Widget() {
    delete ui;
    delete _janelia_config_widget;
}

void Whisker_Widget::openWidget() {

    std::cout << "Whisker Widget Opened" << std::endl;

    // Populate the whisker pad combo box with available PointData

    _data_manager->addObserver([this]() {
        _populateWhiskerPadCombo();
        _populateMaskCombo();
    });

    _createNewWhiskerPad();

    _wt->setWhiskerPadRadius(1000.0f);
    this->show();
}
void Whisker_Widget::_populateMaskCombo() {
    ui->mask_key_combo->blockSignals(true);
    ui->mask_key_combo->clear();

    auto mask_keys = _data_manager->getKeys<MaskData>();
    for (auto const & key: mask_keys) {
        ui->mask_key_combo->addItem(QString::fromStdString(key));
    }

    if (!mask_keys.empty()) {
        if (_selected_mask_key.empty() || std::find(mask_keys.begin(), mask_keys.end(), _selected_mask_key) == mask_keys.end()) {
            _selected_mask_key = mask_keys[0];
        }
        int idx = ui->mask_key_combo->findText(QString::fromStdString(_selected_mask_key));
        if (idx >= 0) ui->mask_key_combo->setCurrentIndex(idx);
    }

    ui->mask_key_combo->blockSignals(false);
}

void Whisker_Widget::closeEvent(QCloseEvent * event) {
    static_cast<void>(event);
    std::cout << "Close event detected" << std::endl;
}

void Whisker_Widget::keyPressEvent(QKeyEvent * event) {

    //Data manager should be responsible for loading new value of data object
    //Main window can update displays with new data object position
    //Frame label is also updated.

    if (event->key() == Qt::Key_T) {
        _traceButton();
    } else {
        //QMainWindow::keyPressEvent(event);
    }
}

/////////////////////////////////////////////

void Whisker_Widget::_traceButton() {

    auto media = _data_manager->getData<MediaData>("media");
    auto const current_position = _state->current_position;

    if (ui->num_frames_to_trace->value() <= 1) {
        auto image = media->getProcessedData8(current_position.convertTo(media->getTimeFrame().get()).getValue());
        if (_use_mask_mode) {
            auto mask_ptr = _data_manager->getData<MaskData>(_selected_mask_key);
            std::vector<uint8_t> binary_mask;
            if (mask_ptr) {
                binary_mask = convert_mask_to_binary(mask_ptr.get(),
                                                     current_position.convertTo(mask_ptr->getTimeFrame().get()).getValue(),
                                                     media->getImageSize());
            }
            auto whiskers = _wt->trace_with_mask(image, binary_mask, media->getHeight(), media->getWidth());

            std::vector<Line2D> whisker_lines(whiskers.size());
            std::transform(whiskers.begin(), whiskers.end(), whisker_lines.begin(), convert_to_Line2D);
            std::for_each(whisker_lines.begin(), whisker_lines.end(), [this](Line2D & line) { clip_whisker(line, _clip_length); });

            std::string const whisker_group_name = "whisker";
            add_whiskers_to_data_manager(
                    _data_manager.get(),
                    whisker_lines,
                    whisker_group_name,
                    _num_whisker_to_track,
                    TimeFrameIndex(current_position.convertTo(media->getTimeFrame().get())),
                    _linking_tolerance);
        } else {
            _traceWhiskers(image, media->getImageSize());
        }
    } else {

        auto height = media->getHeight();
        auto width = media->getWidth();
        int num_to_trace = 0;
        int const start_time = current_position.convertTo(media->getTimeFrame().get()).getValue();

        while (num_to_trace < ui->num_frames_to_trace->value()) {

            auto image = media->getProcessedData8(num_to_trace + current_position.convertTo(media->getTimeFrame().get()).getValue());

            std::vector<whisker::Line2D> whiskers;
            if (_use_mask_mode) {
                auto mask_ptr = _data_manager->getData<MaskData>(_selected_mask_key);
                std::vector<uint8_t> binary_mask;
                if (mask_ptr) {
                    binary_mask = convert_mask_to_binary(mask_ptr.get(), 
                    static_cast<int>(num_to_trace + current_position.convertTo(mask_ptr->getTimeFrame().get()).getValue()), 
                    media->getImageSize());
                }
                whiskers = _wt->trace_with_mask(image, binary_mask, height, width);
            } else {
                whiskers = _wt->trace(image, height, width);
            }

            std::vector<Line2D> whisker_lines(whiskers.size());
            std::transform(whiskers.begin(), whiskers.end(), whisker_lines.begin(), convert_to_Line2D);

            std::for_each(whisker_lines.begin(), whisker_lines.end(), [this](Line2D & line) {
                clip_whisker(line, _clip_length);
            });

            std::string const whisker_group_name = "whisker";

            add_whiskers_to_data_manager(
                    _data_manager.get(),
                    whisker_lines,
                    whisker_group_name,
                    _num_whisker_to_track,
                    TimeFrameIndex(start_time + num_to_trace),
                    _linking_tolerance);

            num_to_trace += 1;
            std::cout << num_to_trace << std::endl;
        }
    }
}

void Whisker_Widget::_dlTraceButton() {

    auto media = _data_manager->getData<MediaData>("media");
    auto const current_position = _state->current_position;
    auto const current_time = current_position.convertTo(media->getTimeFrame().get()).getValue();

    _traceWhiskersDL(media->getProcessedData8(current_time), media->getImageSize());
}

void Whisker_Widget::_dlAddMemoryButton() {

    if (!_data_manager->getData<MaskData>("SAM_output")) {
        _data_manager->setData<MaskData>("SAM_output", TimeKey("time"));
        _data_manager->getData<MaskData>("SAM_output")->setImageSize({.width = 256, .height = 256});
    }

    auto media = _data_manager->getData<MediaData>("media");
    auto const current_position = _state->current_position;
    auto const current_time = current_position.convertTo(media->getTimeFrame().get()).getValue();

    auto image = media->getProcessedData8(current_time);

    dl_model->add_height_width(media->getImageSize());

    std::string const whisker_name = "whisker_" + std::to_string(_current_whisker);

    if (!_data_manager->getData<LineData>(whisker_name)) {
        std::cout << "Whisker named " << whisker_name << " does not exist" << std::endl;
        return;
    }
    auto const whisker = _data_manager->getData<LineData>(whisker_name)->getAtTime(TimeFrameIndex(current_time));

    if (whisker.empty()) {
        std::cout << "No whisker available for " << whisker_name << std::endl;
        return;
    }

    auto memory_mask = line_to_image(whisker[0], media->getHeight(), media->getWidth());

    dl_model->add_memory_frame(image, memory_mask);

    //Debugging
    //auto const width = media->getWidth();
    //auto const height = media->getHeight();
    //QImage labeled_image(&memory_mask[0], width, height, QImage::Format_Grayscale8);
    //labeled_image.save(QString::fromStdString("memory_frame.png"));
}


void Whisker_Widget::_traceWhiskersDL(std::vector<uint8_t> image, ImageSize const image_size) {

    //QElapsedTimer timer3;
    //timer3.start();

    auto mask_output = dl_model->process_frame(image, image_size);

    auto mask_data = _data_manager->getData<MaskData>("SAM_output");

    auto const current_position = _state->current_position;
    auto const current_time = current_position.convertTo(mask_data->getTimeFrame().get()).getValue();

    mask_data->addAtTime(TimeFrameIndex(current_time), mask_output, NotifyObservers::No);
    mask_data->notifyObservers();


    /*
    auto t2 = timer3.elapsed();

    qDebug() << "DL took" << t2;

    smooth_line(output);

    std::string const whisker_name = "whisker_" + std::to_string(_current_whisker);

    _data_manager->getData<LineData>(whisker_name)->clearAtTime(current_time);
    _data_manager->getData<LineData>(whisker_name)->addLineAtTime(current_time, output);
    */

    //Debugging
    //QImage labeled_image(&output[0], 256, 256, QImage::Format_Grayscale8);
    //labeled_image.save(QString::fromStdString("memory_frame.png"));
}

void Whisker_Widget::_traceWhiskers(std::vector<uint8_t> image, ImageSize const image_size) {


    auto const current_position = _state->current_position;
    auto const current_time = current_position.convertTo(_data_manager->getTime(TimeKey("time")).get());
    QElapsedTimer timer2;
    timer2.start();

    auto whiskers = _wt->trace(image, image_size.height, image_size.width);

    std::vector<Line2D> whisker_lines(whiskers.size());
    std::transform(whiskers.begin(), whiskers.end(), whisker_lines.begin(), convert_to_Line2D);

    std::for_each(whisker_lines.begin(), whisker_lines.end(), [this](Line2D & line) {
        clip_whisker(line, _clip_length);
    });

    std::string const whisker_group_name = "whisker";

    add_whiskers_to_data_manager(
            _data_manager.get(),
            whisker_lines,
            whisker_group_name,
            _num_whisker_to_track,
            current_time,
            _linking_tolerance);

    auto t1 = timer2.elapsed();

    qDebug() << "The tracing took" << t1 << "ms";
}

void Whisker_Widget::_changeWhiskerLengthThreshold(double new_threshold) {
    _wt->setWhiskerLengthThreshold(static_cast<float>(new_threshold));
    if (_state) {
        _state->setLengthThreshold(new_threshold);
    }
}

void Whisker_Widget::_selectFaceOrientation(int index) {
    if (index == 0) {
        _face_orientation = Face_Orientation::Facing_Top;
        _wt->setHeadDirection(0.0f, 1.0f);
    } else if (index == 1) {
        _face_orientation = Face_Orientation::Facing_Bottom;
        _wt->setHeadDirection(0.0f, -1.0f);
    } else if (index == 2) {
        _face_orientation = Face_Orientation::Facing_Left;
        _wt->setHeadDirection(1.0f, 0.0f);
    } else {
        _face_orientation = Face_Orientation::Facing_Right;
        _wt->setHeadDirection(-1.0f, 0.0f);
    }
    if (_state) {
        _state->setFaceOrientation(static_cast<FaceOrientation>(index));
    }
}

void Whisker_Widget::_selectNumWhiskersToTrack(int n_whiskers) {
    _num_whisker_to_track = n_whiskers;

    if (_state) {
        _state->setNumWhiskersToTrack(n_whiskers);
    }

    if (n_whiskers == 0) {
        return;
    }

    std::string const whisker_group_name = "whisker";

    ui->manual_whisker_select_spinbox->setMaximum(n_whiskers - 1);

    _createNewWhisker(whisker_group_name, n_whiskers - 1);
}

void Whisker_Widget::_selectWhisker(int whisker_num) {
    _current_whisker = whisker_num;
    if (_state) {
        _state->setCurrentWhisker(whisker_num);
    }
}


/////////////////////////////////////////////

/**
 * @brief Whisker_Widget::_createNewWhisker
 *
 * Whisker name should have the form of (whisker_group_name)_(whisker_number)
 *
 * @param whisker_group_name
 * @param whisker_id
 */
void Whisker_Widget::_createNewWhisker(std::string const & whisker_group_name, int const whisker_id) {
    std::string const whisker_name = whisker_group_name + "_" + std::to_string(whisker_id);

    if (!_data_manager->getData<LineData>(whisker_name)) {
        std::cout << "Creating " << whisker_name << std::endl;
        _data_manager->setData<LineData>(whisker_name, TimeKey("time"));
    }
}

void Whisker_Widget::_loadJaneliaWhiskers() {
    auto janelia_name = QFileDialog::getOpenFileName(
            this,
            "Load Whisker File",
            QDir::currentPath(),
            "All files (*.*) ;; whisker file (*.whiskers)");

    if (janelia_name.isNull()) {
        return;
    }

    auto whiskers_from_janelia = whisker::load_janelia_whiskers(janelia_name.toStdString());

    for (auto & [time, whiskers_in_frame]: whiskers_from_janelia) {
        for (auto & w: whiskers_in_frame) {
            _data_manager->getData<LineData>("unlabeled_whiskers")->addAtTime(TimeFrameIndex(time), convert_to_Line2D(w), NotifyObservers::No);
        }
    }
}


void Whisker_Widget::_openJaneliaConfig() {
    _janelia_config_widget->openWidget();
}

void Whisker_Widget::_changeWhiskerClip(int clip_dist) {
    _clip_length = clip_dist;
    if (_state) {
        _state->setClipLength(clip_dist);
    }

    _traceButton();
}

/////////////////////////////////////////////

/**
 * @brief order_whiskers_by_position
 *
 * Here we arrange the whiskers
 * such that the most posterior whisker is given identity of 1, next most posterior is
 * 2, etc.
 *
 * @param dm
 * @param whisker_group_name
 * @param num_whisker_to_track
 */
void order_whiskers_by_position(
        DataManager * dm,
        std::string const & whisker_group_name,
        int const num_whisker_to_track,
        TimeFrameIndex current_time,
        float similarity_threshold) {

    auto current_index_and_frame = TimeIndexAndFrame(current_time, dm->getTime(TimeKey("time")).get());

    auto const whiskers_view = dm->getData<LineData>("unlabeled_whiskers")->getAtTime(current_time);
    std::vector<Line2D> whiskers(whiskers_view.begin(), whiskers_view.end());

    std::map<int, Line2D> previous_whiskers;
    for (std::size_t i = 0; i < static_cast<std::size_t>(num_whisker_to_track); i++) {

        std::string whisker_name = whisker_group_name + "_" + std::to_string(i);

        if (dm->getData<LineData>(whisker_name)) {
            auto whisker = dm->getData<LineData>(whisker_name)->getAtTime(current_time - TimeFrameIndex(1));
            if (!whisker.empty()) {
                if (!whisker[0].empty()) {
                    previous_whiskers[i] = whisker[0];
                }
            }
        }
    }

    std::map<int, bool> matched_previous;
    for (auto [key, prev_whisker]: previous_whiskers) {
        matched_previous[key] = false;
    }
    std::vector<int> assigned_ids(whiskers.size(), -1);

    for (std::size_t i = 0; i < whiskers.size(); ++i) {
        for (auto [prev_key, prev_whisker]: previous_whiskers) {
            if (matched_previous[prev_key]) continue;

            float const similarity = whisker::fast_discrete_frechet_matrix(
                    convert_to_whisker_Line2D(whiskers[i]),
                    convert_to_whisker_Line2D(prev_whisker));
            std::cout << "Similarity " << similarity << std::endl;
            if (similarity < similarity_threshold) {
                assigned_ids[i] = prev_key;
                matched_previous[prev_key] = true;
                break;
            }
        }
    }

    for (std::size_t i = 0; i < whiskers.size(); ++i) {
        if (assigned_ids[i] != -1) {
            std::string const whisker_name = whisker_group_name + "_" + std::to_string(assigned_ids[i]);
            dm->getData<LineData>(whisker_name)->clearAtTime(current_index_and_frame, NotifyObservers::No);
            dm->getData<LineData>(whisker_name)->addAtTime(current_time, whiskers[i], NotifyObservers::Yes);
        }
    }

    dm->getData<LineData>("unlabeled_whiskers")->clearAtTime(current_index_and_frame, NotifyObservers::No);
    for (std::size_t i = 0; i < whiskers.size(); ++i) {
        if (assigned_ids[i] == -1) {
            dm->getData<LineData>("unlabeled_whiskers")->addAtTime(current_time, whiskers[i], NotifyObservers::Yes);
        }
    }
}

/**
 * @brief add_whiskers_to_data_manager
 *
 *
 * @param dm
 * @param whiskers
 * @param whisker_group_name
 * @param num_whisker_to_track
 */
void add_whiskers_to_data_manager(
        DataManager * dm,
        std::vector<Line2D> & whiskers,
        std::string const & whisker_group_name,
        int const num_whisker_to_track,
        TimeFrameIndex current_time,
        float similarity_threshold) {

    auto current_index_and_frame = TimeIndexAndFrame(current_time, dm->getTime(TimeKey("time")).get());

    dm->getData<LineData>("unlabeled_whiskers")->clearAtTime(current_index_and_frame, NotifyObservers::No);

    for (auto & w: whiskers) {
        dm->getData<LineData>("unlabeled_whiskers")->addAtTime(current_time, w, NotifyObservers::No);
    }

    if (num_whisker_to_track > 0) {
        order_whiskers_by_position(dm, whisker_group_name, num_whisker_to_track, current_time, similarity_threshold);
    }

    dm->getData<LineData>("unlabeled_whiskers")->notifyObservers();
}

void clip_whisker(Line2D & line, int clip_length) {
    if (line.size() <= static_cast<std::size_t>(clip_length)) {
        return;
    }

    line.erase(line.end() - clip_length, line.end());
}

// New whisker pad management methods
void Whisker_Widget::_populateWhiskerPadCombo() {
    ui->whisker_pad_combo->blockSignals(true);
    ui->whisker_pad_combo->clear();

    // Add all existing PointData keys
    auto point_data_keys = _data_manager->getKeys<PointData>();
    for (auto const & key: point_data_keys) {
        ui->whisker_pad_combo->addItem(QString::fromStdString(key));
    }

    ui->whisker_pad_combo->blockSignals(false);

    // Select the _current_whisker_pad_key
    if (!_current_whisker_pad_key.empty()) {
        ui->whisker_pad_combo->setCurrentText(QString::fromStdString(_current_whisker_pad_key));
    }
}

void Whisker_Widget::_updateWhiskerPadFromSelection() {
    QString selected_text = ui->whisker_pad_combo->currentText();

    if (selected_text.toStdString() == _current_whisker_pad_key) {
        return;
    }

    if (_whisker_pad_callback_id != -1) {
        _data_manager->getData<PointData>(_current_whisker_pad_key)->removeObserver(_whisker_pad_callback_id);
    }

    _current_whisker_pad_key = selected_text.toStdString();
    if (_state) {
        _state->setWhiskerPadKey(_current_whisker_pad_key);
    }

    // Update the frame spinbox range based on available data
    auto point_data = _data_manager->getData<PointData>(_current_whisker_pad_key);
    if (point_data) {
        auto times_with_data = point_data->getTimesWithData();
        if (!times_with_data.empty()) {
            ui->whisker_pad_frame_spinbox->blockSignals(true);

            // Set range to include all available times
            auto min_time = *std::min_element(times_with_data.begin(), times_with_data.end());
            auto max_time = *std::max_element(times_with_data.begin(), times_with_data.end());
            ui->whisker_pad_frame_spinbox->setMinimum(min_time.getValue());
            ui->whisker_pad_frame_spinbox->setMaximum(max_time.getValue());

            // Set to current time if available, otherwise first available time
            auto const current_position = _state->current_position;
            auto const current_time = current_position.convertTo(point_data->getTimeFrame().get());
            if (std::find(times_with_data.begin(), times_with_data.end(), current_time) != times_with_data.end()) {
                ui->whisker_pad_frame_spinbox->setValue(current_time.getValue());
            } else {
                ui->whisker_pad_frame_spinbox->setValue(min_time.getValue());
            }

            ui->whisker_pad_frame_spinbox->blockSignals(false);
        }

        _whisker_pad_callback_id = point_data->addObserver([this]() {
            _updateWhiskerPadLabel();
        });
    }

    _updateWhiskerPadLabel();
}

void Whisker_Widget::_updateWhiskerPadLabel() {
    if (_current_whisker_pad_key.empty()) {
        ui->whisker_pad_pos_label->setText("(0,0)");
        _current_whisker_pad_point = {0.0f, 0.0f};
        return;
    }

    auto point_data = _data_manager->getData<PointData>(_current_whisker_pad_key);
    if (!point_data) {
        ui->whisker_pad_pos_label->setText("(0,0)");
        _current_whisker_pad_point = {0.0f, 0.0f};
        return;
    }

    int frame = ui->whisker_pad_frame_spinbox->value();
    auto points_at_frame = point_data->getAtTime(TimeFrameIndex(frame));

    if (points_at_frame.empty()) {
        ui->whisker_pad_pos_label->setText("(no data)");
        _current_whisker_pad_point = {0.0f, 0.0f};
    } else {
        // Use the first point if multiple points exist
        Point2D<float> whisker_pad_point = points_at_frame[0];
        _current_whisker_pad_point = whisker_pad_point;

        std::string const whisker_pad_label =
                "(" + std::to_string(static_cast<int>(whisker_pad_point.x)) + "," +
                std::to_string(static_cast<int>(whisker_pad_point.y)) + ")";
        ui->whisker_pad_pos_label->setText(QString::fromStdString(whisker_pad_label));

        // Update the whisker tracker and DL model with the new position
        _wt->setWhiskerPad(whisker_pad_point.x, whisker_pad_point.y);
        dl_model->add_origin(whisker_pad_point.x, whisker_pad_point.y);

        // Update state with whisker pad position
        if (_state) {
            _state->setWhiskerPadPosition(whisker_pad_point.x, whisker_pad_point.y);
        }

        std::cout << "Whisker pad set to: (" << whisker_pad_point.x << ", " << whisker_pad_point.y << ")" << std::endl;
    }
}

void Whisker_Widget::_createNewWhiskerPad() {
    std::string const new_key = "whisker_pad";

    // Check if "whisker_pad" already exists
    if (!_data_manager->getData<PointData>(new_key)) {
        _data_manager->setData<PointData>(new_key, TimeKey("time"));
        std::cout << "Created new PointData: " << new_key << std::endl;

        // Create a new point at frame one with value 0,0
        _data_manager->getData<PointData>(new_key)->addAtTime(TimeFrameIndex(0), {0.0f, 0.0f}, NotifyObservers::No);

        // Repopulate the combo box to include the new entry
        _populateWhiskerPadCombo();

        // Select the newly created whisker_pad
        int index = ui->whisker_pad_combo->findText("whisker_pad");
        if (index >= 0) {
            ui->whisker_pad_combo->setCurrentIndex(index);
        }
    } else {
        // If it already exists, just select it
        int index = ui->whisker_pad_combo->findText("whisker_pad");
        if (index >= 0) {
            ui->whisker_pad_combo->setCurrentIndex(index);
        } else {
            // If not in combo box, repopulate
            _populateWhiskerPadCombo();
            index = ui->whisker_pad_combo->findText("whisker_pad");
            if (index >= 0) {
                ui->whisker_pad_combo->setCurrentIndex(index);
            }
        }
    }

    _updateWhiskerPadFromSelection();
}

void Whisker_Widget::_onWhiskerPadComboChanged(QString const & text) {
    std::cout << "Whisker pad selection changed to: " << text.toStdString() << std::endl;
    _updateWhiskerPadFromSelection();
}

void Whisker_Widget::_onWhiskerPadFrameChanged(int frame) {
    std::cout << "Whisker pad frame changed to: " << frame << std::endl;
    _updateWhiskerPadLabel();
}
