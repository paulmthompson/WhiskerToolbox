
#include "Whisker_Widget.hpp"

#include "ui_Whisker_Widget.h"

#include "contact_widget.hpp"

#include "DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/Lines/IO/Binary/Line_Data_Binary.hpp"
#include "DataManager/Lines/IO/CSV/Line_Data_CSV.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "IO_Widgets/Media/media_export.hpp"
#include "Magic_Eraser_Widget/magic_eraser.hpp"
#include "Media_Window.hpp"
#include "TimeFrame.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"
#include "janelia_config.hpp"
#include "mainwindow.hpp"
#include "utils/opencv_utility.hpp"
#include "utils/string_manip.hpp"
#include "whiskertracker.hpp"


#include "opencv2/core/mat.hpp"
#include "qevent.h"
#include <QElapsedTimer>
#include <QFileDialog>

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots

#include <torch/script.h>
#include <torch/torch.h>

#define slots Q_SLOTS

#include "utils/Deep_Learning/scm.hpp"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>


std::vector<std::string> whisker_colors = {"#ff0000", // Red
                                           "#008000", // Green
                                           "#00ffff", // Cyan
                                           "#ff00ff", // Magenta
                                           "#ffff00"};// Yellow

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
 * @brief Whisker_Widget::Whisker_Widget
 *
 *
 *
 * @param scene
 * @param data_manager
 * @param time_scrollbar
 * @param mainwindow
 * @param parent
 */
Whisker_Widget::Whisker_Widget(Media_Window * scene,
                               std::shared_ptr<DataManager> data_manager,
                               TimeScrollBar * time_scrollbar,
                               MainWindow * main_window,
                               QWidget * parent)
    : QMainWindow(parent),
      _wt{std::make_shared<whisker::WhiskerTracker>()},
      _scene{scene},
      _data_manager{std::move(data_manager)},
      _time_scrollbar{time_scrollbar},
      _main_window{main_window},
      _output_path{std::filesystem::current_path()},
      ui(new Ui::Whisker_Widget) {
    ui->setupUi(this);

    ui->output_dir_label->setText(QString::fromStdString(std::filesystem::current_path().string()));

    _data_manager->setData<LineData>("unlabeled_whiskers");
    _addDrawingCallback("unlabeled_whiskers");

    _janelia_config_widget = new Janelia_Config(_wt);

    dl_model = std::make_unique<dl::SCM>();

    if (!_data_manager->getData<DigitalEventSeries>("tracked_frames")) {
        _data_manager->setData<DigitalEventSeries>("tracked_frames");
    }

    connect(ui->trace_button, &QPushButton::clicked, this, &Whisker_Widget::_traceButton);
    connect(ui->dl_trace_button, &QPushButton::clicked, this, &Whisker_Widget::_dlTraceButton);
    connect(ui->dl_add_memory_button, &QPushButton::clicked, this, &Whisker_Widget::_dlAddMemoryButton);
    connect(ui->actionJanelia_Settings, &QAction::triggered, this, &Whisker_Widget::_openJaneliaConfig);
    connect(ui->face_orientation, &QComboBox::currentIndexChanged, this, &Whisker_Widget::_selectFaceOrientation);
    connect(ui->length_threshold_spinbox, &QDoubleSpinBox::valueChanged, this,
            &Whisker_Widget::_changeWhiskerLengthThreshold);
    connect(ui->whisker_pad_select, &QPushButton::clicked, this, &Whisker_Widget::_selectWhiskerPad);
    connect(ui->whisker_number, &QSpinBox::valueChanged, this, &Whisker_Widget::_selectNumWhiskersToTrack);

    connect(ui->manual_whisker_select_spinbox, &QSpinBox::valueChanged, this, &Whisker_Widget::_selectWhisker);
    connect(ui->delete_whisker_button, &QPushButton::clicked, this, &Whisker_Widget::_deleteWhisker);

    connect(ui->actionSave_Face_Mask_2, &QAction::triggered, this, &Whisker_Widget::_saveFaceMask);
    connect(ui->actionLoad_Face_Mask, &QAction::triggered, this, &Whisker_Widget::_loadFaceMask);
    connect(ui->export_image_csv, &QPushButton::clicked, this, &Whisker_Widget::_exportImageCSV);

    connect(ui->actionLoad_Janelia_Whiskers, &QAction::triggered, this, &Whisker_Widget::_loadJaneliaWhiskers);

    connect(ui->actionLoad_CSV_Whiskers, &QAction::triggered, this, &Whisker_Widget::_loadSingleCSVWhisker);
    connect(ui->actionLoad_CSV_Whiskers_Multiple, &QAction::triggered, this, &Whisker_Widget::_loadMultiCSVWhiskers);

    connect(ui->actionSave_as_CSV, &QAction::triggered, this, &Whisker_Widget::_saveWhiskersAsCSV);
    connect(ui->actionSave_as_Binary, &QAction::triggered, this, &Whisker_Widget::_saveWhiskersAsBinary);
    connect(ui->actionLoad_CSV_Whisker_Single_File_Multi_Frame, &QAction::triggered, this, &Whisker_Widget::_loadMultiFrameCSV);

    connect(ui->actionOpen_Contact_Detection, &QAction::triggered, this, &Whisker_Widget::_openContactWidget);

    connect(ui->mask_dilation, &QSpinBox::valueChanged, this, &Whisker_Widget::_maskDilation);

    connect(ui->output_dir_button, &QPushButton::clicked, this, &Whisker_Widget::_changeOutputDir);

    connect(ui->whisker_clip, &QSpinBox::valueChanged, this, &Whisker_Widget::_changeWhiskerClip);

    connect(ui->magic_eraser_button, &QPushButton::clicked, this, &Whisker_Widget::_magicEraserButton);

    connect(ui->export_all_tracked, &QPushButton::clicked, this, &Whisker_Widget::_exportAllTracked);

    connect(ui->auto_dl_checkbox, &QCheckBox::stateChanged, this, [this]() {
        if (ui->auto_dl_checkbox->isChecked()) {
            _auto_dl = true;
        } else {
            _auto_dl = false;
        }
    });

    connect(ui->linking_tol_spinbox, &QSpinBox::valueChanged, this, [this](int val) {
        _linking_tolerance = static_cast<float>(val);
    });

    connect(ui->select_whisker_button, &QPushButton::clicked, this, [this]() {
        _selection_mode = Whisker_Select;
    });

};

Whisker_Widget::~Whisker_Widget() {
    delete ui;
    delete _janelia_config_widget;
}

void Whisker_Widget::openWidget() {

    std::cout << "Whisker Widget Opened" << std::endl;

    connect(_scene, SIGNAL(leftClick(qreal, qreal)), this, SLOT(_clickedInVideo(qreal, qreal)));
    connect(_scene, &Media_Window::leftRelease, this, &Whisker_Widget::_drawingFinished);

    this->show();
}

void Whisker_Widget::closeEvent(QCloseEvent * event) {
    std::cout << "Close event detected" << std::endl;

    disconnect(_scene, SIGNAL(leftClick(qreal, qreal)), this, SLOT(_clickedInVideo(qreal, qreal)));
}

void Whisker_Widget::keyPressEvent(QKeyEvent * event) {

    //Data manager should be responsible for loading new value of data object
    //Main window can update displays with new data object position
    //Frame label is also updated.

    if (event->key() == Qt::Key_T) {
        _traceButton();
    } else if (event->key() == Qt::Key_E) {
        _exportImageCSV();
    } else {
        //QMainWindow::keyPressEvent(event);
    }
}

/////////////////////////////////////////////

void Whisker_Widget::_traceButton() {

    auto media = _data_manager->getData<MediaData>("media");
    auto current_time = _data_manager->getTime()->getLastLoadedFrame();

    if (ui->num_frames_to_trace->value() <= 1) {
        _traceWhiskers(media->getProcessedData(current_time), media->getImageSize());
    } else {

        auto height = media->getHeight();
        auto width = media->getWidth();
        int num_to_trace = 0;
        int const start_time = current_time;

        while (num_to_trace < ui->num_frames_to_trace->value()) {

            auto image = media->getProcessedData(num_to_trace + current_time);

            auto whiskers = _wt->trace(image, height, width);

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
                    start_time + num_to_trace,
                    _linking_tolerance);

            num_to_trace += 1;
            std::cout << num_to_trace << std::endl;

        }
    }
}

void Whisker_Widget::_dlTraceButton() {

    auto media = _data_manager->getData<MediaData>("media");
    auto current_time = _data_manager->getTime()->getLastLoadedFrame();

    _traceWhiskersDL(media->getProcessedData(current_time), media->getImageSize());
}

void Whisker_Widget::_dlAddMemoryButton() {

    if (!_data_manager->getData<MaskData>("SAM_output")) {
        _data_manager->setData<MaskData>("SAM_output");
        _data_manager->getData<MaskData>("SAM_output")->setImageSize({.width=256, .height=256});
        _addDrawingCallback("SAM_output");
    }

    auto media = _data_manager->getData<MediaData>("media");
    auto current_time = _data_manager->getTime()->getLastLoadedFrame();

    auto image = media->getProcessedData(current_time);

    dl_model->add_height_width(media->getImageSize());

    std::string const whisker_name = "whisker_" + std::to_string(_current_whisker);

    if (!_data_manager->getData<LineData>(whisker_name)) {
        std::cout << "Whisker named " << whisker_name << " does not exist" << std::endl;
        return;
    }
    auto whisker = _data_manager->getData<LineData>(whisker_name)->getLinesAtTime(current_time);

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

    auto const current_time = _data_manager->getTime()->getLastLoadedFrame();

    _data_manager->getData<MaskData>("SAM_output")->addAtTime(current_time, mask_output);


    /*
    auto t2 = timer3.elapsed();

    qDebug() << "DL took" << t2;

    smooth_line(output);

    std::string const whisker_name = "whisker_" + std::to_string(_current_whisker);

    _data_manager->getData<LineData>(whisker_name)->clearLinesAtTime(current_time);
    _data_manager->getData<LineData>(whisker_name)->addLineAtTime(current_time, output);
    */

    //Debugging
    //QImage labeled_image(&output[0], 256, 256, QImage::Format_Grayscale8);
    //labeled_image.save(QString::fromStdString("memory_frame.png"));
}

void Whisker_Widget::_traceWhiskers(std::vector<uint8_t> image, ImageSize const image_size) {
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
            _data_manager->getTime()->getLastLoadedFrame(),
            _linking_tolerance);

    auto t1 = timer2.elapsed();

    qDebug() << "The tracing took" << t1 << "ms";
}

void Whisker_Widget::_selectWhiskerPad() {
    _selection_mode = Selection_Type::Whisker_Pad_Select;
}

void Whisker_Widget::_changeWhiskerLengthThreshold(double new_threshold) {
    _wt->setWhiskerLengthThreshold(static_cast<float>(new_threshold));
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
}

void Whisker_Widget::_selectNumWhiskersToTrack(int n_whiskers) {
    _num_whisker_to_track = n_whiskers;

    if (n_whiskers == 0) {
        return;
    }

    std::string const whisker_group_name = "whisker";

    ui->manual_whisker_select_spinbox->setMaximum(n_whiskers - 1);

    _createNewWhisker(whisker_group_name, n_whiskers - 1);
}

void Whisker_Widget::_selectWhisker(int whisker_num) {
    _current_whisker = whisker_num;
}

void Whisker_Widget::_deleteWhisker() {
    std::string const whisker_group_name = "whisker";

    std::string const whisker_name = whisker_group_name + "_" + std::to_string(_current_whisker);

    auto current_time = _data_manager->getTime()->getLastLoadedFrame();

    if (_data_manager->getData<LineData>(whisker_name)) {
        _data_manager->getData<LineData>(whisker_name)->clearLinesAtTime(current_time);
        if (_data_manager->getData<DigitalEventSeries>("tracked_frames")) {
            _data_manager->getData<DigitalEventSeries>("tracked_frames")->removeEvent(current_time);
        }
    }
}

/////////////////////////////////////////////

void Whisker_Widget::_saveImage(std::string const & folder) {

    MediaExportOptions opts;
    opts.save_by_frame_name = _save_by_frame_name;
    opts.frame_id_padding = 7;
    opts.image_name_prefix = "img";
    opts.image_save_dir = folder;

    auto media = _data_manager->getData<MediaData>("media");
    auto const frame_id = _data_manager->getTime()->getLastLoadedFrame();

    save_image(media.get(), frame_id, opts);
}

void Whisker_Widget::_saveFaceMask() {

    auto const media_data = _data_manager->getData<MediaData>("media");

    auto const image_size = ImageSize{.width = media_data->getWidth(), .height = media_data->getHeight()};

    auto const frame_id = _data_manager->getTime()->getLastLoadedFrame();

    auto mask = media_data->getRawData(frame_id);

    auto m2 = convert_vector_to_mat(mask, image_size);

    median_blur(m2, 35);

    //TODO I can use OpenCV to save image here instead of converting back to vector

    convert_mat_to_vector(mask, m2, image_size);

    auto mask_image = QImage(&mask[0],
                             image_size.width,
                             image_size.height,
                             QImage::Format_Grayscale8);

    std::string const saveName = "mask" + pad_frame_id(frame_id, 7) + ".png";
    std::cout << "Saving file " << saveName << std::endl;

    mask_image.save(QString::fromStdString(saveName));
}

void Whisker_Widget::_loadFaceMask() {
    auto face_mask_name = QFileDialog::getOpenFileName(
            this,
            "Load Face Mask",
            QDir::currentPath(),
            "All files (*.*)");

    if (face_mask_name.isNull()) {
        return;
    }

    auto mat = load_mask_from_image(face_mask_name.toStdString());

    auto mask_original = std::make_shared<MaskData>();
    mask_original->setImageSize({mat.cols, mat.rows});
    _data_manager->setData<MaskData>("Face_Mask_Original", mask_original);

    auto mask_points_original = create_mask(mat);
    _data_manager->getData<MaskData>("Face_Mask_Original")->addAtTime(-1, mask_points_original);


    auto mask = std::make_shared<MaskData>();
    mask->setImageSize({mat.cols, mat.rows});
    _data_manager->setData<MaskData>("Face_Mask", mask);

    int const dilation_size = 5;
    grow_mask(mat, dilation_size);

    auto mask_points = create_mask(mat);

    auto face_mask = _data_manager->getData<MaskData>("Face_Mask");

    //std::cout << "Mask has " << mask_points.size() << " pixels " <<  std::endl;

    face_mask->addAtTime(-1, mask_points);

    ui->mask_file_label->setText(face_mask_name);

    _addDrawingCallback("Face_Mask");
}


/*

Single frame whisker saving

*/
void Whisker_Widget::_saveWhiskerAsCSV(std::string const & folder, std::vector<Point2D<float>> const & whisker) {
    auto const frame_id = _data_manager->getTime()->getLastLoadedFrame();

    auto saveName = _getWhiskerSaveName(frame_id);

    save_line_as_csv(whisker, folder + saveName);
}

/*

Save whisker in all frames

*/

void Whisker_Widget::_saveWhiskersAsCSV() {
    std::string const whisker_group_name = "whisker";
    std::string const whisker_name = whisker_group_name + "_" + std::to_string(_current_whisker);

    auto line_data = _data_manager->getData<LineData>(whisker_name);

    save_lines_csv(line_data.get(), whisker_name + ".csv");
}

void Whisker_Widget::_saveWhiskersAsBinary() {
    std::string const whisker_group_name = "whisker";
    std::string const whisker_name = whisker_group_name + "_" + std::to_string(_current_whisker);

    auto line_data = _data_manager->getData<LineData>(whisker_name);

    std::string filepath = whisker_name + ".bin";

    auto binary_saver = BinaryFileCapnpStorage();

    QElapsedTimer timer2;
    timer2.start();

    binary_saver.save(*line_data.get(), filepath);

    auto t1 = timer2.elapsed();

    qDebug() << "The saving took" << t1 << "ms";
    std::cout << "Saved to LMDB at whisker.lmdb" << std::endl;
}

int get_whisker_id(std::string const & whisker_name) {
    std::string number = "";
    for (auto c: whisker_name) {
        if (isdigit(c)) {
            number += c;
        }
    }

    return std::stoi(number);
}

void Whisker_Widget::_loadMultiFrameCSV() {
    auto whisker_filepath = QFileDialog::getOpenFileName(
            this,
            "Load Whisker CSV",
            QDir::currentPath(),
            "All files (*.*)");

    auto line_map = load_line_csv(whisker_filepath.toStdString());

    //Get the whisker name from the filename using filesystem
    auto whisker_filename = std::filesystem::path(whisker_filepath.toStdString()).filename().string();

    //Remove .csv suffix from filename
    auto whisker_name = remove_extension(whisker_filename);

    std::cout << "Creating whisker " << whisker_name << std::endl;

    _data_manager->setData<LineData>(whisker_name, std::make_shared<LineData>(line_map));

    _addDrawingCallback(whisker_name);
}

std::string Whisker_Widget::_getWhiskerSaveName(int const frame_id) {

    if (_save_by_frame_name) {
        auto media = _data_manager->getData<MediaData>("media");
        auto frame_string = media->GetFrameID(frame_id);
        frame_string = remove_extension(frame_string);

        //Strip off the img prefix and leave the number


        return frame_string + ".csv";
    } else {

        std::string saveName = pad_frame_id(frame_id, 7) + ".csv";
        return saveName;
    }
}


void Whisker_Widget::_exportAllTracked() {

    if (_num_whisker_to_track == 0) {
        std::cout << "No whiskers selected to track" << std::endl;
        return;
    }
    std::string const whisker_group_name = "whisker";

    std::string const whisker_name = whisker_group_name + "_" + std::to_string(_current_whisker);

    auto whisker_id = get_whisker_id(whisker_name);

    std::string const whisker_folder = _output_path.string() + "/" + std::to_string(whisker_id) + "/";
    std::filesystem::create_directory(whisker_folder);

    auto whiskers = _data_manager->getData<LineData>(whisker_name);
    auto media = _data_manager->getData<MediaData>("media");
    auto const width = media->getWidth();
    auto const height = media->getHeight();

    auto tracked_frames = _data_manager->getData<DigitalEventSeries>("tracked_frames")->getEventSeries();

    MediaExportOptions opts;
    opts.save_by_frame_name = _save_by_frame_name;
    opts.frame_id_padding = 7;
    opts.image_name_prefix = "img";
    opts.image_save_dir = _output_path.string();

    for (auto const & event_frame : tracked_frames)
    {
        int const frame_id = static_cast<int>(event_frame);

        if (whiskers->getLinesAtTime(frame_id).empty()) {
            continue;
        }

        save_image(media.get(), frame_id, opts);

        auto saveName = _getWhiskerSaveName(frame_id);

        auto frame_whiskers = whiskers->getLinesAtTime(frame_id);

        save_line_as_csv(frame_whiskers[0], whisker_folder + saveName);
    }
}

/**
 * @brief Whisker_Widget::_exportImageCSV
 *
 *
 */
void Whisker_Widget::_exportImageCSV() {

    std::string const whisker_group_name = "whisker";

    if (!check_whisker_num_matches_export_num(_data_manager.get(), _num_whisker_to_track, whisker_group_name)) {
        return;
    }

    _saveImage(_output_path.string());

    auto current_time = _data_manager->getTime()->getLastLoadedFrame();

    _addNewTrackedWhisker(current_time);


    for (int i = 0; i < _num_whisker_to_track; i++) {
        std::string const whisker_name = whisker_group_name + "_" + std::to_string(i);

        if (_data_manager->getData<LineData>(whisker_name)) {

            auto whiskers = _data_manager->getData<LineData>(whisker_name)->getLinesAtTime(current_time);

            std::string const whisker_folder = _output_path.string() + "/" + std::to_string(i) + "/";
            std::filesystem::create_directory(whisker_folder);

            _saveWhiskerAsCSV(whisker_folder, whiskers[0]);
        }
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
        _data_manager->setData<LineData>(whisker_name);

        _addDrawingCallback(whisker_name);
    }
}

void Whisker_Widget::_clickedInVideo(qreal x_canvas, qreal y_canvas) {

    auto scene = dynamic_cast<Media_Window *>(sender());

    float const x_media = static_cast<float>(x_canvas) / scene->getXAspect();
    float const y_media = static_cast<float>(y_canvas) / scene->getYAspect();

    auto current_time = _data_manager->getTime()->getLastLoadedFrame();

    switch (_selection_mode) {
        case Whisker_Select: {

            auto whiskers = _data_manager->getData<LineData>("unlabeled_whiskers")->getLinesAtTime(current_time);
            std::tuple<float, int> nearest_whisker = whisker::get_nearest_whisker(
                    convert_to_whisker_Line2D(whiskers),
                    x_media,
                    y_media);
            if (std::get<0>(nearest_whisker) < 10.0f) {
                _selected_whisker = std::get<1>(nearest_whisker);

                std::string const whisker_group_name = "whisker_" + std::to_string(_current_whisker);
                if (_data_manager->getData<LineData>(whisker_group_name)) {
                    _data_manager->getData<LineData>(whisker_group_name)->addLineAtTime(current_time, whiskers[_selected_whisker]);

                    _data_manager->getData<LineData>("unlabeled_whiskers")->clearLineAtTime(current_time, _selected_whisker);

                    if (_data_manager->getData<DigitalEventSeries>("tracked_frames")) {
                        _data_manager->getData<DigitalEventSeries>("tracked_frames")->addEvent(current_time);
                    }
                }
            }
            break;
        }

        case Whisker_Pad_Select: {
            _wt->setWhiskerPad(x_media, y_media);
            dl_model->add_origin(x_media, y_media);
            std::string const whisker_pad_label =
                    "(" + std::to_string(static_cast<int>(x_media)) + "," + std::to_string(static_cast<int>(y_media)) +
                    ")";
            ui->whisker_pad_pos_label->setText(QString::fromStdString(whisker_pad_label));
            _selection_mode = Whisker_Select;
            break;
        }
        default:
            break;
    }
    auto * contact_widget = _main_window->getWidget("Contact_Widget");
    if (contact_widget) {
        dynamic_cast<Contact_Widget *>(contact_widget)->setPolePos(x_media, y_media);// Pass forward to contact widget
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
            _data_manager->getData<LineData>("unlabeled_whiskers")->addLineAtTime(time, convert_to_Line2D(w));
        }
    }
}


/**
 * @brief Whisker_Widget::_loadCSVWhiskerFromDir
 *
 * Loads whisker lines where each is defined in a CSV file in
 * the same directory. The filename of the CSV file should
 * correspond to the frame number of the corresponding video
 *
 * The CSV is assumed
 * to have x positions in column 1 and y positions in column 2.
 * Each row should correspond to a single point moving from follicle
 * to whisker tip.
 *
 * @param dir_name
 * @param whisker_group_name
 *
 * @return vector of frame numbers that were loaded
 */
std::vector<int> Whisker_Widget::_loadCSVWhiskerFromDir(std::string const & dir_name, std::string const & whisker_group_name) {
    auto dir_path = std::filesystem::path(dir_name);
    auto const whisker_number = std::stoi(dir_path.filename().string());

    _createNewWhisker(whisker_group_name, whisker_number);

    std::string const whisker_name = whisker_group_name + "_" + std::to_string(whisker_number);

    auto loaded_frames = load_csv_lines_into_data_manager(_data_manager.get(), dir_name, whisker_name);

    return loaded_frames;
}


/**
 *
 * Loads multiple CSV files located in the same directory
 * CSV files are assumed to contain the frame number in the
 * corresponding video. And each CSV should contain two columns
 * with the 1st column being pixel positions in x and 2nd column
 * being pixel positions in y.
 *
 * Each row
 *
 * @brief Whisker_Widget::_loadSingleCSVWhisker
 */
void Whisker_Widget::_loadSingleCSVWhisker() {
    auto const dir_name = QFileDialog::getExistingDirectory(
                                  this,
                                  "Load Whisker CSV Files",
                                  QDir::currentPath())
                                  .toStdString();

    if (dir_name.empty()) {
        return;
    }

    std::string const whisker_group_name = "whisker";

    auto loaded_whisker_ids = _loadCSVWhiskerFromDir(dir_name, whisker_group_name);

    _addNewTrackedWhisker(loaded_whisker_ids);
}

void Whisker_Widget::_loadMultiCSVWhiskers() {
    auto const dir_name = QFileDialog::getExistingDirectory(
                                  this,
                                  "Load Whisker CSV Files",
                                  QDir::currentPath())
                                  .toStdString();

    if (dir_name.empty()) {
        return;
    }

    std::string const whisker_group_name = "whisker";

    std::vector<int> loaded_whisker_ids;
    for (auto const & entry: std::filesystem::directory_iterator(dir_name)) {
        if (entry.is_directory()) {
            loaded_whisker_ids = _loadCSVWhiskerFromDir(entry.path().string(), whisker_group_name);
        }
    }

    _addNewTrackedWhisker(loaded_whisker_ids);
}

void Whisker_Widget::_openJaneliaConfig() {
    _janelia_config_widget->openWidget();
}

void Whisker_Widget::_openContactWidget() {
    std::string const key = "Contact_Widget";

    auto * contact_widget = _main_window->getWidget(key);
    if (!contact_widget) {
        auto contact_widget_ptr = std::make_unique<Contact_Widget>(_data_manager, _time_scrollbar);
        _main_window->addWidget(key, std::move(contact_widget_ptr));

        contact_widget = _main_window->getWidget(key);
        _main_window->registerDockWidget(key, contact_widget, ads::RightDockWidgetArea);
        dynamic_cast<Contact_Widget *>(contact_widget)->openWidget();
    } else {
        dynamic_cast<Contact_Widget *>(contact_widget)->openWidget();
    }

    _main_window->showDockWidget(key);
}

void Whisker_Widget::LoadFrame(int frame_id) {
    auto * contact_widget = _main_window->getWidget("Contact_Widget");
    if (contact_widget) {
        dynamic_cast<Contact_Widget *>(contact_widget)->updateFrame(frame_id);
    }
    if (_auto_dl) {
        _dlTraceButton();
    }
}

/**
 * @brief Whisker_Widget::_addNewTrackedWhisker
 *
 * Adds a new whisker to the list of tracked whiskers
 *
 * @param index
 */
void Whisker_Widget::_addNewTrackedWhisker(int const index) {
    auto tracked_frames = _data_manager->getData<DigitalEventSeries>("tracked_frames");
    tracked_frames->addEvent(static_cast<float>(index));
}

void Whisker_Widget::_addNewTrackedWhisker(std::vector<int> const & indexes) {
    auto tracked_frames = _data_manager->getData<DigitalEventSeries>("tracked_frames");

    for (auto const index: indexes) {
        tracked_frames->addEvent(static_cast<float>(index));
    }
}


void Whisker_Widget::_maskDilation(int dilation_size) {

    if (!_data_manager->getData<MaskData>("Face_Mask_Original")) {
        return;
    }

    int const time = -1;

    auto original_mask = _data_manager->getData<MaskData>("Face_Mask_Original");

    auto mask_pixels = original_mask->getAtTime(time)[0];

    //convert mask to opencv
    auto mat = convert_vector_to_mat(mask_pixels, original_mask->getImageSize());

    grow_mask(mat, dilation_size);

    auto new_mask = create_mask(mat);

    auto dilated_mask = _data_manager->getData<MaskData>("Face_Mask");

    dilated_mask->clearAtTime(time);

    dilated_mask->addAtTime(time, new_mask);

    auto mask_for_tracker = std::vector<whisker::Point2D<float>>();
    for (auto const & p: new_mask) {
        mask_for_tracker.push_back(whisker::Point2D<float>{p.x, p.y});
    }
    _wt->setFaceMask(mask_for_tracker);
}

void Whisker_Widget::_maskDilationExtended(int dilation_size) {
}

void Whisker_Widget::_magicEraserButton() {
    _selection_mode = Selection_Type::Magic_Eraser;
    _scene->setDrawingMode(true);
}

void Whisker_Widget::_drawingFinished() {
    switch (_selection_mode) {
        case Magic_Eraser: {
            std::cout << "Drawing finished" << std::endl;

            auto media = _data_manager->getData<MediaData>("media");

            auto mask = _scene->getDrawingMask();

            auto frame_id = _data_manager->getTime()->getLastLoadedFrame();

            auto image = media->getRawData(frame_id);
            auto const image_size = media->getImageSize();

            auto erased = apply_magic_eraser(image, image_size, mask);

            _traceWhiskers(erased, image_size);

            _selection_mode = Whisker_Select;
            _scene->setDrawingMode(false);
            break;
        }
        default:
            break;
    }
}

void Whisker_Widget::_changeOutputDir() {
    QString const dir_name = QFileDialog::getExistingDirectory(
            this,
            "Select Directory",
            QDir::currentPath());

    if (dir_name.isEmpty()) {
        return;
    }

    _output_path = std::filesystem::path(dir_name.toStdString());
    ui->output_dir_label->setText(dir_name);
}

void Whisker_Widget::_changeWhiskerClip(int clip_dist) {
    _clip_length = clip_dist;

    _traceButton();
}

void Whisker_Widget::_addDrawingCallback(std::string data_name) {
    _data_manager->addCallbackToData(data_name, [this]() {
        _scene->UpdateCanvas();
    });
}

/////////////////////////////////////////////

/**
 * @brief load_csv_lines_into_data_manager
 * @param dm
 * @param dir_name
 * @param line_key
 * @return vector of loaded frame IDs
 */
std::vector<int> load_csv_lines_into_data_manager(DataManager * dm, std::string const & dir_name, std::string const & line_key) {
    auto loaded_frames = std::vector<int>{};

    for (auto const & entry: std::filesystem::directory_iterator(dir_name)) {
        auto const frame_num = remove_extension(entry.path().filename().string());
        auto whisker = load_line_from_csv(entry.path().string());

        //Find the relative frame corresponding to this frame number.
        auto media = dm->getData<MediaData>("media");
        auto const frame_index = media->getFrameIndexFromNumber(std::stoi(frame_num));

        dm->getData<LineData>(line_key)->addLineAtTime(frame_index, whisker);
        loaded_frames.push_back(frame_index);
    }

    std::cout << "Loaded " << loaded_frames.size() << " whiskers" << std::endl;

    return loaded_frames;
}

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
        int current_time,
        float similarity_threshold) {

    std::vector<Line2D> whiskers = dm->getData<LineData>("unlabeled_whiskers")->getLinesAtTime(current_time);

    std::map<int, Line2D> previous_whiskers;
    for (std::size_t i = 0; i < static_cast<std::size_t>(num_whisker_to_track); i++) {

        std::string whisker_name = whisker_group_name + "_" + std::to_string(i);

        if (dm->getData<LineData>(whisker_name)) {
            auto whisker = dm->getData<LineData>(whisker_name)->getLinesAtTime(current_time - 1);
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
            dm->getData<LineData>(whisker_name)->clearLinesAtTime(current_time);
            dm->getData<LineData>(whisker_name)->addLineAtTime(current_time, whiskers[i]);
        }
    }
    /*
    int next_id = 0;
    for (std::size_t i = 0; i < whiskers.size(); ++i) {
        if (assigned_ids[i] == -1) {
            while (std::find(assigned_ids.begin(), assigned_ids.end(), next_id) != assigned_ids.end()) {
                ++next_id;
            }
            if (next_id >= num_whisker_to_track) {
                continue;
            }
            std::string whisker_name = whisker_group_name + "_" + std::to_string(next_id);
            dm->getData<LineData>(whisker_name)->clearLinesAtTime(current_time);
            dm->getData<LineData>(whisker_name)->addLineAtTime(current_time, whiskers[i]);
            assigned_ids[i] = next_id;
        }
    }
    */

    dm->getData<LineData>("unlabeled_whiskers")->clearLinesAtTime(current_time);
    for (std::size_t i = 0; i < whiskers.size(); ++i) {
        if (assigned_ids[i] == -1) {
            dm->getData<LineData>("unlabeled_whiskers")->addLineAtTime(current_time, whiskers[i]);
        }
    }

    /*
    for (std::size_t i = 0; i < static_cast<std::size_t>(num_whisker_to_track); i++) {

        if (i >= whiskers.size()) {
            break;
        }

        std::string whisker_name = whisker_group_name + "_" + std::to_string(i);

        dm->getData<LineData>(whisker_name)->clearLinesAtTime(current_time);
        dm->getData<LineData>(whisker_name)->addLineAtTime(current_time, whiskers[i]);
    }

    dm->getData<LineData>("unlabeled_whiskers")->clearLinesAtTime(current_time);

    std::cout << "The size of remaining whiskers is " << whiskers.size() << std::endl;

    for (std::size_t i = static_cast<std::size_t>(num_whisker_to_track); i < whiskers.size(); i++) {
        dm->getData<LineData>("unlabeled_whiskers")->addLineAtTime(current_time, whiskers[i]);
    }
*/
}

/**
 * @brief check_whisker_num_matches_export_num
 *
 * This checks if the number of whiskers that have been ordered in the frame is equal
 * to the number of whiskers we have set to track in the image.
 *
 * This prevents misclicks where we click export and only 4 whiskers may be present
 * but we wish to track 5.
 *
 * @param dm
 * @param num_whiskers_to_export
 *
 * @return
 */
bool check_whisker_num_matches_export_num(DataManager * dm, int const num_whiskers_to_export, std::string const & whisker_group_name) {

    auto current_time = dm->getTime()->getLastLoadedFrame();

    int whiskers_in_frame = 0;

    for (int i = 0; i < num_whiskers_to_export; i++) {
        std::string const whisker_name = whisker_group_name + "_" + std::to_string(i);

        if (dm->getData<LineData>(whisker_name)) {
            auto whiskers = dm->getData<LineData>(whisker_name)->getLinesAtTime(current_time);
            if (!whiskers.empty()) {
                if (!whiskers[0].empty()) {
                    whiskers_in_frame += 1;
                }
            }
        }
    }

    std::cout << "There are " << whiskers_in_frame << " whiskers in this image" << std::endl;

    if (whiskers_in_frame != num_whiskers_to_export) {
        std::cout << "There are " << whiskers_in_frame << " in image, but " << num_whiskers_to_export << " are supposed to be tracked" << std::endl;
        return false;
    } else {
        return true;
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
        int current_time,
        float similarity_threshold) {
    dm->getData<LineData>("unlabeled_whiskers")->clearLinesAtTime(current_time);

    for (auto & w: whiskers) {
        dm->getData<LineData>("unlabeled_whiskers")->addLineAtTime(current_time, w);
    }

    if (num_whisker_to_track > 0) {
        order_whiskers_by_position(dm, whisker_group_name, num_whisker_to_track, current_time, similarity_threshold);
    }
}

void clip_whisker(Line2D & line, int clip_length) {
    if (clip_length <= 0 || clip_length > line.size()) {
        return;// Invalid clip length, do nothing
    }
    line.erase(line.end() - clip_length, line.end());
}
