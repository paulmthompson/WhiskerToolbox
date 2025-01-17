
#include "Whisker_Widget.hpp"

#include "contact_widget.hpp"

#include "DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"

#include "janelia_config.hpp"
#include "mainwindow.hpp"
#include "Media_Window.hpp"
#include "TimeFrame.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"
#include "ui_Whisker_Widget.h"
#include "utils/opencv_utility.hpp"
#include "utils/string_manip.hpp"
#include "whiskertracker.hpp"
#include "Magic_Eraser_Widget/magic_eraser.hpp"

#include <QFileDialog>
#include <QElapsedTimer>
#include "qevent.h"
#include "opencv2/core/mat.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots

#include <torch/torch.h>
#include <torch/script.h>

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
                                            "#ffff00"}; // Yellow

Line2D& convert_to_Line2D(whisker::Line2D& whisker_line) {
    return reinterpret_cast<Line2D&>(whisker_line);
}

// Function to convert Line2D to whisker::Line2D without copying
whisker::Line2D& convert_to_whisker_Line2D(Line2D& line) {
    return reinterpret_cast<whisker::Line2D&>(line);
}

std::vector<whisker::Line2D>& convert_to_whisker_Line2D(std::vector<Line2D>& lines) {
    return reinterpret_cast<std::vector<whisker::Line2D>&>(lines);
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
Whisker_Widget::Whisker_Widget(Media_Window *scene,
                               std::shared_ptr<DataManager> data_manager,
                               TimeScrollBar* time_scrollbar,
                               MainWindow* mainwindow,
                               QWidget *parent) :
        QMainWindow(parent),
        _wt{std::make_shared<whisker::WhiskerTracker>()},
        _scene{scene},
        _data_manager{data_manager},
        _time_scrollbar{time_scrollbar},
        _main_window{mainwindow},
        _output_path{std::filesystem::current_path()},
        ui(new Ui::Whisker_Widget)
{
    ui->setupUi(this);

    ui->output_dir_label->setText(QString::fromStdString(std::filesystem::current_path().string()));

    _data_manager->setData<LineData>("unlabeled_whiskers");
    _scene->addLineDataToScene("unlabeled_whiskers");
    _scene->changeLineColor("unlabeled_whiskers","#0000ff");

    _addDrawingCallback("unlabeled_data");
    _janelia_config_widget = new Janelia_Config(_wt);

    dl_model = std::make_unique<dl::SCM>();

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
    connect(ui->manual_whisker_button, &QPushButton::clicked, this, &Whisker_Widget::_manualWhiskerToggle);


    connect(ui->actionSave_Snapshot, &QAction::triggered, this, &Whisker_Widget::_saveImageButton);
    connect(ui->actionSave_Face_Mask_2, &QAction::triggered, this, &Whisker_Widget::_saveFaceMask);
    connect(ui->actionLoad_Face_Mask, &QAction::triggered, this, &Whisker_Widget::_loadFaceMask);
    connect(ui->export_image_csv, &QPushButton::clicked, this, &Whisker_Widget::_exportImageCSV);

    connect(ui->actionLoad_Janelia_Whiskers, &QAction::triggered, this, &Whisker_Widget::_loadJaneliaWhiskers);

    connect(ui->actionLoad_Mask,  &QAction::triggered, this, &Whisker_Widget::_loadHDF5WhiskerMask);
    connect(ui->actionLoad_HDF5_Mask_Multiple, &QAction::triggered, this, &Whisker_Widget::_loadHDF5WhiskerMasksFromDir);

    connect(ui->actionLoad_HDF5_Whisker_Single, &QAction::triggered, this, &Whisker_Widget::_loadHDF5WhiskerLine);
    connect(ui->actionLoad_HDF5_Whisker_Multiple, &QAction::triggered, this, &Whisker_Widget::_loadHDF5WhiskerLinesFromDir);

    connect(ui->actionLoad_CSV_Whiskers, &QAction::triggered, this, &Whisker_Widget::_loadSingleCSVWhisker);
    connect(ui->actionLoad_CSV_Whiskers_Multiple, &QAction::triggered, this, &Whisker_Widget::_loadMultiCSVWhiskers);

    connect(ui->actionSave_as_CSV, &QAction::triggered, this, &Whisker_Widget::_saveWhiskersAsCSV);
    connect(ui->actionLoad_CSV_Whisker_Single_File_Multi_Frame, &QAction::triggered, this, &Whisker_Widget::_loadMultiFrameCSV);

    connect(ui->actionLoad_Keypoint_CSV, &QAction::triggered, this, &Whisker_Widget::_loadKeypointCSV);

    connect(ui->actionOpen_Contact_Detection, &QAction::triggered, this, &Whisker_Widget::_openContactWidget);

    connect(ui->tracked_whisker_number, &QSpinBox::valueChanged, this, &Whisker_Widget::_skipToTrackedFrame);

    connect(ui->mask_dilation, &QSpinBox::valueChanged, this, &Whisker_Widget::_maskDilation);

    connect(ui->output_dir_button, &QPushButton::clicked, this, &Whisker_Widget::_changeOutputDir);

    connect(ui->whisker_clip, &QSpinBox::valueChanged, this, &Whisker_Widget::_changeWhiskerClip);

    connect(ui->magic_eraser_button, &QPushButton::clicked, this, &Whisker_Widget::_magicEraserButton);

    connect(ui->export_all_tracked, &QPushButton::clicked, this, &Whisker_Widget::_exportAllTracked);

    connect(ui->auto_dl_checkbox, &QCheckBox::stateChanged, this, [this](){
        if (ui->auto_dl_checkbox->isChecked()) {
            _auto_dl = true;
        } else {
            _auto_dl = false;
        }
    });

    connect(ui->linking_tol_spinbox, &QSpinBox::valueChanged, this, [this](int val){
        _linking_tolerance = static_cast<float>(val);
    });

    connect(ui->select_whisker_button, &QPushButton::clicked, this, [this]() {
        _selection_mode = Whisker_Select;
    });

    connect(ui->manual_whisker_lock_frame, &QSpinBox::valueChanged, this, &Whisker_Widget::_setLockFrame);

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

void Whisker_Widget::closeEvent(QCloseEvent *event) {
    std::cout << "Close event detected" << std::endl;

    disconnect(_scene, SIGNAL(leftClick(qreal, qreal)), this, SLOT(_clickedInVideo(qreal, qreal)));
}

void Whisker_Widget::keyPressEvent(QKeyEvent *event) {

    //Data manager should be responsible for loading new value of data object
    //Main window can update displays with new data object position
    //Frame label is also updated.

    if (event->key() == Qt::Key_T) {
        _traceButton();
    } else if (event->key() == Qt::Key_E){
        _exportImageCSV();
    } else {
        //QMainWindow::keyPressEvent(event);

    }
}

/////////////////////////////////////////////

void Whisker_Widget::_traceButton() {
    QElapsedTimer timer2;
    timer2.start();

    auto media = _data_manager->getData<MediaData>("media");
    auto current_time = _data_manager->getTime()->getLastLoadedFrame();

    if (ui->num_frames_to_trace->value() <= 1) {
        _traceWhiskers(media->getProcessedData(current_time), media->getHeight(), media->getWidth());
    } else {

        auto height = media->getHeight(); auto width = media->getWidth();
        int num_to_trace = 0;
        int start_time = current_time;

        while (num_to_trace < ui->num_frames_to_trace->value()) {

            auto image = media->getProcessedData(num_to_trace + current_time);

            auto whiskers = _wt->trace(image, height, width);

            std::vector<Line2D> whisker_lines(whiskers.size());
            std::transform(whiskers.begin(), whiskers.end(), whisker_lines.begin(), convert_to_Line2D);

            std::for_each(whisker_lines.begin(), whisker_lines.end(), [this](Line2D& line) {
                clip_whisker(line, _clip_length);
            });

            std::string whisker_group_name = "whisker";

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

void Whisker_Widget::_dlTraceButton()
{

    auto media = _data_manager->getData<MediaData>("media");
    auto current_time = _data_manager->getTime()->getLastLoadedFrame();

    _traceWhiskersDL(media->getProcessedData(current_time), media->getHeight(), media->getWidth());

}

void Whisker_Widget::_dlAddMemoryButton()
{
    auto media = _data_manager->getData<MediaData>("media");
    auto current_time = _data_manager->getTime()->getLastLoadedFrame();

    auto image = media->getProcessedData(current_time);

    dl_model->add_height_width(media->getHeight(), media->getWidth());

    std::string whisker_name = "whisker_" + std::to_string(_current_whisker);

    if (!_data_manager->getData<LineData>(whisker_name))
    {
        std::cout << "Whisker named " << whisker_name << " does not exist" << std::endl;
        return;
    }
    auto whisker = _data_manager->getData<LineData>(whisker_name)->getLinesAtTime(current_time);

    if (whisker.size() == 0) {
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


void Whisker_Widget::_traceWhiskersDL(std::vector<uint8_t> image, int height, int width)
{

    QElapsedTimer timer3;
    timer3.start();

    auto output = dl_model->process_frame(image, height, width);

    std::transform(output.begin(), output.end(), output.begin(), [height, width](Point2D<float> p) {
        return Point2D<float>{p.x / 256 * width, p.y / 256 * height};
    });

    auto t2 = timer3.elapsed();

    qDebug() << "DL took" << t2;

    smooth_line(output);

    std::string whisker_name = "whisker_" + std::to_string(_current_whisker);

    auto current_time = _data_manager->getTime()->getLastLoadedFrame();
    _data_manager->getData<LineData>(whisker_name)->clearLinesAtTime(current_time);
    _data_manager->getData<LineData>(whisker_name)->addLineAtTime(current_time, output);

    //Debugging
    //QImage labeled_image(&output[0], 256, 256, QImage::Format_Grayscale8);
    //labeled_image.save(QString::fromStdString("memory_frame.png"));
}

void Whisker_Widget::_traceWhiskers(std::vector<uint8_t> image, int height, int width)
{
    QElapsedTimer timer2;
    timer2.start();

    auto whiskers = _wt->trace(image, height, width);

    std::vector<Line2D> whisker_lines(whiskers.size());
    std::transform(whiskers.begin(), whiskers.end(), whisker_lines.begin(), convert_to_Line2D);

    std::for_each(whisker_lines.begin(), whisker_lines.end(), [this](Line2D& line) {
        clip_whisker(line, _clip_length);
    });

    std::string whisker_group_name = "whisker";

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

    std::string whisker_group_name = "whisker";

    ui->manual_whisker_select_spinbox->setMaximum(n_whiskers - 1);

    _createNewWhisker(whisker_group_name, n_whiskers-1);
}

void Whisker_Widget::_selectWhisker(int whisker_num) {
    _current_whisker = whisker_num;

}

void Whisker_Widget::_setLockFrame(int lock_frame)
{
    std::string whisker_group_name = "whisker";

    std::string whisker_name = whisker_group_name + "_" + std::to_string(_current_whisker);

    auto whisker = _data_manager->getData<LineData>(whisker_name);
    whisker->lockUntil(lock_frame);
}

void Whisker_Widget::_deleteWhisker()
{
    std::string whisker_group_name = "whisker";

    std::string whisker_name = whisker_group_name + "_" + std::to_string(_current_whisker);

    auto current_time = _data_manager->getTime()->getLastLoadedFrame();

    if (_data_manager->getData<LineData>(whisker_name)) {
        _data_manager->getData<LineData>(whisker_name)->clearLinesAtTime(current_time);
    }
}

void Whisker_Widget::_manualWhiskerToggle()
{
    //check toggle state
    if (_selection_mode == Selection_Type::Manual_Trace) {
        _selection_mode = Selection_Type::Whisker_Select;
    } else {
        _selection_mode = Selection_Type::Manual_Trace;
    }
}

/////////////////////////////////////////////

void Whisker_Widget::_saveImageButton() {
    _saveImage(_output_path.string() + "/");
}

void Whisker_Widget::_saveImage(std::string const& folder)
{
    auto media = _data_manager->getData<MediaData>("media");
    auto const frame_id = _data_manager->getTime()->getLastLoadedFrame();

    auto media_data = media->getRawData(frame_id);

    auto const width = media->getWidth();
    auto const height = media->getHeight();

    QImage labeled_image(&media_data[0], width, height, QImage::Format_Grayscale8);

    auto saveName = _getImageSaveName(frame_id);

    std::cout << "Saving file " << saveName << std::endl;

    labeled_image.save(QString::fromStdString(folder + saveName));
}

void Whisker_Widget::_saveFaceMask() {

    auto const media_data = _data_manager->getData<MediaData>("media");

    auto const width = media_data->getWidth();
    auto const height = media_data->getHeight();

    auto const frame_id = _data_manager->getTime()->getLastLoadedFrame();

    auto mask = media_data->getRawData(frame_id);

    auto m2 = convert_vector_to_mat(mask, width, height);

    median_blur(m2, 35);

    //TODO I can use OpenCV to save image here instead of converting back to vector

    convert_mat_to_vector(mask, m2, width, height);

    auto mask_image = QImage(&mask[0],
                                 width,
                                 height,
                                  QImage::Format_Grayscale8
                                 );

    std::string saveName = "mask" + pad_frame_id(frame_id, 7) + ".png";
    std::cout << "Saving file " << saveName << std::endl;

    mask_image.save(QString::fromStdString(saveName));
}

void Whisker_Widget::_loadFaceMask()
{
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
    mask_original->setMaskWidth(mat.cols);
    mask_original->setMaskHeight(mat.rows);
    _data_manager->setData<MaskData>("Face_Mask_Original", mask_original);

    auto mask_points_original = create_mask(mat);
    _data_manager->getData<MaskData>("Face_Mask_Original")->addMaskAtTime(-1,mask_points_original);


    auto mask = std::make_shared<MaskData>();
    mask->setMaskWidth(mat.cols);
    mask->setMaskHeight(mat.rows);
    _data_manager->setData<MaskData>("Face_Mask", mask);

    const int dilation_size = 5;
    grow_mask(mat, dilation_size);

    auto mask_points = create_mask(mat);

    auto face_mask = _data_manager->getData<MaskData>("Face_Mask");

    //std::cout << "Mask has " << mask_points.size() << " pixels " <<  std::endl;

    face_mask->addMaskAtTime(-1,mask_points);

    _scene->addMaskDataToScene("Face_Mask");
    _scene->changeMaskColor("Face_Mask", "#808080");

    ui->mask_file_label->setText(face_mask_name);

    _addDrawingCallback("Face_Mask");
}


/*

Single frame whisker saving

*/
void Whisker_Widget::_saveWhiskerAsCSV(const std::string& folder, const std::vector<Point2D<float>>& whisker)
{
    auto const frame_id = _data_manager->getTime()->getLastLoadedFrame();

    auto saveName = _getWhiskerSaveName(frame_id);

    save_line_as_csv(whisker, folder + saveName);
}

/*

Save whisker in all frames

*/

void Whisker_Widget::_saveWhiskersAsCSV()
{
    std::string whisker_group_name = "whisker";
    std::string whisker_name = whisker_group_name + "_" + std::to_string(_current_whisker);

    auto data = _data_manager->getData<LineData>(whisker_name)->getData();

    save_lines_csv(data, whisker_name + ".csv");
}

int get_whisker_id(std::string whisker_name)
{
    std::string number = "";
    for (auto c : whisker_name) {
        if (isdigit(c)) {
            number += c;
        }
    }

    return std::stoi(number);
}

void Whisker_Widget::_loadMultiFrameCSV()
{
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

    //If there is a number at the end of whisker_name, that is the whisker_id
    auto whisker_id = get_whisker_id(whisker_name);

    _scene->addLineDataToScene(whisker_name);
    _scene->changeLineColor(whisker_name, get_whisker_color(whisker_id));

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


void Whisker_Widget::_exportAllTracked()
{
    std::string const whisker_group_name = "whisker";

    std::string whisker_name = whisker_group_name + "_" + std::to_string(_current_whisker);

    auto whisker_id = get_whisker_id(whisker_name);

    std::string image_folder = _output_path.string() + "/images/";
    std::filesystem::create_directory(image_folder);

    std::string whisker_folder = _output_path.string() + "/" + std::to_string(whisker_id) + "/";
    std::filesystem::create_directory(whisker_folder);

    auto whiskers = _data_manager->getData<LineData>(whisker_name)->getData();
    auto media = _data_manager->getData<MediaData>("media");
    auto const width = media->getWidth();
    auto const height = media->getHeight();

    auto start_frame = ui->export_all_start_spinbox->value();
    auto last_frame = ui->export_all_end_spinbox->value();

    for (auto & whisker_pair : whiskers) {
        int frame_id = whisker_pair.first;

        if ((frame_id < start_frame) | (frame_id > last_frame)) {
            continue;
        }

        auto media_data = media->getRawData(frame_id);

        QImage labeled_image(&media_data[0], width, height, QImage::Format_Grayscale8);

        auto saveName = _getImageSaveName(frame_id);

        std::cout << "Saving file " << saveName << std::endl;

        labeled_image.save(QString::fromStdString(image_folder + saveName));

        saveName = _getWhiskerSaveName(frame_id);

        save_line_as_csv(whisker_pair.second[0], whisker_folder+ saveName);


    }
}

/**
 * @brief Whisker_Widget::_exportImageCSV
 *
 *
 */
void Whisker_Widget::_exportImageCSV()
{

    std::string const whisker_group_name = "whisker";

    if (!_checkWhiskerNumMatchesExportNum(_data_manager.get(), _num_whisker_to_track, whisker_group_name)) {
        return;
    }

    std::string folder = _output_path.string() + "/images/";

    std::filesystem::create_directory(folder);
    _saveImage(folder);

    auto current_time = _data_manager->getTime()->getLastLoadedFrame();

    _addNewTrackedWhisker(current_time);


    for (int i = 0; i<_num_whisker_to_track; i++)
    {
        std::string whisker_name = whisker_group_name + "_" + std::to_string(i);

        if (_data_manager->getData<LineData>(whisker_name)) {

            auto whiskers = _data_manager->getData<LineData>(whisker_name)->getLinesAtTime(current_time);

            std::string whisker_folder = _output_path.string() + "/" + std::to_string(i) + "/";
            std::filesystem::create_directory(whisker_folder);

            _saveWhiskerAsCSV(whisker_folder, whiskers[0]);
        }
    }
}

std::string Whisker_Widget::_getImageSaveName(int const frame_id)
{
    if (_save_by_frame_name)
    {
        auto media = _data_manager->getData<MediaData>("media");
        auto saveName = media->GetFrameID(frame_id);
        return saveName;
    } else {

        std::string saveName = "img" + pad_frame_id(frame_id, 7) + ".png";
        return saveName;
    }
}

void Whisker_Widget::_loadKeypointCSV()
{
    auto keypoint_filename = QFileDialog::getOpenFileName(
        this,
        "Load Keypoints",
        QDir::currentPath(),
        "All files (*.*)");

    if (keypoint_filename.isNull()) {
        return;
    }

    auto keypoints = load_points_from_csv(keypoint_filename.toStdString(), 0, 1, 2);

    auto point_num = _data_manager->getKeys<PointData>().size();

    std::cout << "There are " << point_num << " keypoints loaded" << std::endl;

    auto keypoint_key = "keypoint_" + std::to_string(point_num);

    _data_manager->setData<PointData>(keypoint_key);

    auto point = _data_manager->getData<PointData>(keypoint_key);
    auto media = _data_manager->getData<MediaData>("media");

    point->setMaskHeight(media->getHeight());
    point->setMaskWidth(media->getWidth());

    for (auto & [key, val] : keypoints) {
        point->addPointAtTime(media->getFrameIndexFromNumber(key), val.x, val.y);
    }

    _scene->addPointDataToScene(keypoint_key);
    _scene->changePointColor(keypoint_key, get_whisker_color(point_num));
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
void Whisker_Widget::_createNewWhisker(std::string const & whisker_group_name, const int whisker_id)
{
    std::string const whisker_name = whisker_group_name + "_" + std::to_string(whisker_id);

    if (!_data_manager->getData<LineData>(whisker_name)) {
        std::cout << "Creating " << whisker_name << std::endl;
        _data_manager->setData<LineData>(whisker_name);
        _scene->addLineDataToScene(whisker_name);
        _scene->changeLineColor(whisker_name, get_whisker_color(whisker_id));

        _addDrawingCallback(whisker_name);
    }
}

void Whisker_Widget::_clickedInVideo(qreal x_canvas, qreal y_canvas) {

    auto scene = dynamic_cast<Media_Window*>(sender());

    float x_media = x_canvas / scene->getXAspect();
    float y_media = y_canvas / scene->getYAspect();

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

                std::string whisker_group_name = "whisker_" + std::to_string(_current_whisker);
                if (_data_manager->getData<LineData>(whisker_group_name)) {
                    _data_manager->getData<LineData>(whisker_group_name)->addLineAtTime(current_time, whiskers[_selected_whisker]);
                }
            }
            break;

        }

        case Whisker_Pad_Select: {
            _wt->setWhiskerPad(x_media,y_media);
            dl_model->add_origin(x_media, y_media);
            std::string whisker_pad_label =
                    "(" + std::to_string(static_cast<int>(x_media)) + "," + std::to_string(static_cast<int>(y_media)) +
                    ")";
            ui->whisker_pad_pos_label->setText(QString::fromStdString(whisker_pad_label));
            _selection_mode = Whisker_Select;
            break;
        }

        case Manual_Trace: {

            std::string whisker_group_name = "whisker";
            std::string whisker_name = whisker_group_name + "_" + std::to_string(_current_whisker);

            if (_data_manager->getData<LineData>(whisker_name)) {
                 _data_manager->getData<LineData>(whisker_name)->addPointToLineInterpolate(
                         current_time,
                         0,
                         x_media,
                         y_media);

                scene->UpdateCanvas();
            }

            break;
        }
        default:
            break;
    }
    auto* contact_widget = _main_window->getWidget("Contact_Widget");
    if (contact_widget) {
        dynamic_cast<Contact_Widget*>(contact_widget)->setPolePos(x_media,y_media); // Pass forward to contact widget
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

    for (auto &[time, whiskers_in_frame]: whiskers_from_janelia) {
        for (auto &w: whiskers_in_frame) {
            _data_manager->getData<LineData>("unlabeled_whiskers")->addLineAtTime(time, convert_to_Line2D(w));
        }
    }
}


/**
 * @brief Whisker_Widget::_loadHDF5WhiskerMasks
 *
 *
 */
void Whisker_Widget::_loadHDF5WhiskerMask()
{
    auto filename = QFileDialog::getOpenFileName(
        this,
        "Load Whisker File",
        QDir::currentPath(),
        "All files (*.*)");

    if (filename.isNull()) {
        return;
    }

    _loadSingleHDF5WhiskerMask(filename.toStdString());
}

/**
 * @brief Whisker_Widget::_loadHDF5WhiskerMasksFromDir
 *
 *
 */
void Whisker_Widget::_loadHDF5WhiskerMasksFromDir()
{

    QString dir_name = QFileDialog::getExistingDirectory(
        this,
        "Select Directory",
        QDir::currentPath());

    if (dir_name.isEmpty()) {
        return;
    }

    std::filesystem::path directory(dir_name.toStdString());

    // Store the paths of all files that match the criteria
    std::vector<std::filesystem::path> whisker_files;

    for (const auto & entry : std::filesystem::directory_iterator(directory)) {
        std::string filename = entry.path().filename().string();
        if (filename.find("_whisker.h5") != std::string::npos) {
            whisker_files.push_back(entry.path());
        }
    }

    // Sort the files based on their names
    std::sort(whisker_files.begin(), whisker_files.end());

    // Load the files in sorted order
    for (const auto & file : whisker_files) {
        _loadSingleHDF5WhiskerMask(file.string());
    }
}

/**
 * @brief Whisker_Widget::_loadSingleHDF5WhiskerMask
 *
 * Loads a whisker mask defined in a HDF5 file (e.g. a bag of points
 * that define the pixels in an image that represent a whisker with
 * NO ordering).
 *
 * @param filename
 */
void Whisker_Widget::_loadSingleHDF5WhiskerMask(std::string const & filename)
{
    auto frames =  read_array_hdf5(filename, "frames");
    auto probs = read_ragged_hdf5(filename, "probs");
    auto y_coords = read_ragged_hdf5(filename, "heights");
    auto x_coords = read_ragged_hdf5(filename, "widths");

    auto mask_num = _data_manager->getKeys<MaskData>().size();

    auto mask_key = "Whisker_Mask" + std::to_string(mask_num);

    _data_manager->setData<MaskData>(mask_key);

    auto mask = _data_manager->getData<MaskData>(mask_key);

    for (std::size_t i = 0; i < frames.size(); i ++) {
        mask->addMaskAtTime(frames[i], x_coords[i], y_coords[i]);
    }

    _scene->addMaskDataToScene(mask_key);
    _scene->changeMaskColor(mask_key, get_whisker_color(mask_num));
}

/**
 * @brief Whisker_Widget::_loadHDF5WhiskerLine
 *
 * Creates a dialog box for the user to select a single hdf5 file
 * that defines whisker *lines*
 *
 */
void Whisker_Widget::_loadHDF5WhiskerLine()
{
    auto filename = QFileDialog::getOpenFileName(
        this,
        "Load Whisker File",
        QDir::currentPath(),
        "All files (*.*)");

    if (filename.isNull()) {
        return;
    }

    std::string whisker_group_name = "whisker";

    int whisker_num = 0;

    _loadSingleHDF5WhiskerLine(filename.toStdString(), whisker_group_name, whisker_num);
}

/**
 * @brief Whisker_Widget::_loadHDF5WhiskerLinesFromDir
 *
 * Creates a dialog box for the user to select a directory
 * containing one or multiple hdf5 files that define
 * whisker *lines*
 *
 * This will look for entries following the format
 * whisker_x where x is the whisker ID number.
 *
 * 0 signifies most posterior
 *
 */
void Whisker_Widget::_loadHDF5WhiskerLinesFromDir()
{
    QString dir_name = QFileDialog::getExistingDirectory(
            this,
            "Select Directory",
            QDir::currentPath());

    if (dir_name.isEmpty()) {
        return;
    }

    std::filesystem::path directory(dir_name.toStdString());

    // Store the paths of all files that match the criteria
    std::vector<std::filesystem::path> whisker_files;

    for (const auto & entry : std::filesystem::directory_iterator(directory)) {
        std::string filename = entry.path().filename().string();

        if (filename.find("whisker_") != std::string::npos && filename.find(".h5") != std::string::npos) {
            whisker_files.push_back(entry.path());
        }
    }

    // Sort the files based on their names
    std::sort(whisker_files.begin(), whisker_files.end());

    std::string whisker_group_name = "whisker";

    // Load the files in sorted order
    int whisker_num = 0;
    for (const auto & file : whisker_files) {
        _loadSingleHDF5WhiskerLine(file.string(), whisker_group_name, whisker_num);
        whisker_num += 1;
    }
}


/**
 * @brief Whisker_Widget::_loadSingleHDF5WhiskerLine
 *
 *
 *
 * @param filename
 * @param whisker_group_name
 * @param whisker_num
 */
void Whisker_Widget::_loadSingleHDF5WhiskerLine(std::string const & filename, std::string const & whisker_group_name, int const whisker_num)
{

    _createNewWhisker(whisker_group_name, whisker_num);

    std::string const whisker_name = whisker_group_name + "_" + std::to_string(whisker_num);

    read_hdf5_line_into_datamanager(_data_manager.get(), filename, whisker_name);
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
std::vector<int> Whisker_Widget::_loadCSVWhiskerFromDir(std::string const & dir_name, std::string const & whisker_group_name)
{
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
void Whisker_Widget::_loadSingleCSVWhisker()
{
    auto const dir_name =  QFileDialog::getExistingDirectory(
        this,
        "Load Whisker CSV Files",
        QDir::currentPath()).toStdString();

    if (dir_name.empty()) {
        return;
    }

    std::string whisker_group_name = "whisker";

    auto loaded_whisker_ids = _loadCSVWhiskerFromDir(dir_name, whisker_group_name);

    _addNewTrackedWhisker(loaded_whisker_ids);
}

void Whisker_Widget::_loadMultiCSVWhiskers()
{
    auto const dir_name =  QFileDialog::getExistingDirectory(
                              this,
                              "Load Whisker CSV Files",
                              QDir::currentPath()).toStdString();

    if (dir_name.empty()) {
        return;
    }

    std::string whisker_group_name = "whisker";

    std::vector<int> loaded_whisker_ids;
    for (const auto & entry : std::filesystem::directory_iterator(dir_name))
    {
        if (entry.is_directory())
        {
            loaded_whisker_ids = _loadCSVWhiskerFromDir(entry.path().string(), whisker_group_name);
        }
    }

    _addNewTrackedWhisker(loaded_whisker_ids);

}

void Whisker_Widget::_openJaneliaConfig()
{
    _janelia_config_widget->openWidget();
}

void Whisker_Widget::_openContactWidget()
{
    std::string const key = "Contact_Widget";

    auto* contact_widget = _main_window->getWidget(key);
    if (!contact_widget) {
        auto contact_widget_ptr = std::make_unique<Contact_Widget>(_data_manager, _time_scrollbar);
        _main_window->addWidget(key, std::move(contact_widget_ptr));

        contact_widget = _main_window->getWidget(key);
        _main_window->registerDockWidget(key, contact_widget, ads::RightDockWidgetArea);
        dynamic_cast<Contact_Widget*>(contact_widget)->openWidget();
    } else {
        dynamic_cast<Contact_Widget*>(contact_widget)->openWidget();
    }

    _main_window->showDockWidget(key);
}

void Whisker_Widget::LoadFrame(int frame_id)
{
    auto* contact_widget = _main_window->getWidget("Contact_Widget");
    if (contact_widget) {
        dynamic_cast<Contact_Widget*>(contact_widget)->updateFrame(frame_id);
    }
    if (_auto_dl) {
        _dlTraceButton();
    }
}

/**
 * @brief Whisker_Widget::_skipToTrackedFrame
 *
 * A tracked frame is one where we have generated labels.
 * IDs of tracked frames are stored in _tracked_frame_ids
 *
 *
 * @param index
 */
void Whisker_Widget::_skipToTrackedFrame(int index)
{
    if (index < 0 || index >= _tracked_frame_ids.size()) return;

    auto it = _tracked_frame_ids.begin();
    std::advance(it, index);

    int frame_id = *it;
    _time_scrollbar->changeScrollBarValue(frame_id);
}

/**
 * @brief Whisker_Widget::_addNewTrackedWhisker
 *
 * Adds a new whisker to the list of tracked whiskers
 *
 * @param index
 */
void Whisker_Widget::_addNewTrackedWhisker(int const index)
{
    _tracked_frame_ids.insert(index);

    ui->tracked_whisker_number->setMaximum(_tracked_frame_ids.size() - 1);

    ui->tracked_whisker_count->setText(QString::number(_tracked_frame_ids.size()));
}

void Whisker_Widget::_addNewTrackedWhisker(std::vector<int> const & indexes)
{
    for (auto i : indexes) {
        _tracked_frame_ids.insert(i);
    }

    ui->tracked_whisker_number->setMaximum(_tracked_frame_ids.size() - 1);

    ui->tracked_whisker_count->setText(QString::number(_tracked_frame_ids.size()));
}


void Whisker_Widget::_maskDilation(int dilation_size)
{

    if (!_data_manager->getData<MaskData>("Face_Mask_Original"))
    {
        return;
    }

    int const time = -1;

    auto original_mask = _data_manager->getData<MaskData>("Face_Mask_Original");

    auto mask_pixels = original_mask->getMasksAtTime(time)[0];

    //convert mask to opencv
    auto mat = convert_vector_to_mat(mask_pixels, original_mask->getMaskWidth(), original_mask->getMaskHeight());

    grow_mask(mat, dilation_size);

    auto new_mask = create_mask(mat);

    auto dilated_mask = _data_manager->getData<MaskData>("Face_Mask");

    dilated_mask->clearMasksAtTime(time);

    dilated_mask->addMaskAtTime(time, new_mask );

    auto mask_for_tracker = std::vector<whisker::Point2D<float>>();
    for (auto const & p : new_mask) {
        mask_for_tracker.push_back(whisker::Point2D<float>{p.x, p.y});
    }
    _wt->setFaceMask(mask_for_tracker);
}

void Whisker_Widget::_maskDilationExtended(int dilation_size)
{

}

void Whisker_Widget::_magicEraserButton()
{
    _selection_mode = Selection_Type::Magic_Eraser;
    _scene->setDrawingMode(true);
}

void Whisker_Widget::_drawingFinished()
{
    switch (_selection_mode) {
        case Magic_Eraser: {
            std::cout << "Drawing finished" << std::endl;

            auto media = _data_manager->getData<MediaData>("media");

            auto mask = _scene->getDrawingMask();

            auto frame_id = _data_manager->getTime()->getLastLoadedFrame();

            auto image = media->getRawData(frame_id);
            auto height = media->getHeight();
            auto width = media->getWidth();

            auto erased = apply_magic_eraser(image,width,height,mask);

            _traceWhiskers(erased, height, width);

            _selection_mode = Whisker_Select;
            _scene->setDrawingMode(false);
            break;
        }
        default:
            break;
    }
}

void Whisker_Widget::_changeOutputDir()
{
    QString dir_name = QFileDialog::getExistingDirectory(
        this,
        "Select Directory",
        QDir::currentPath());

    if (dir_name.isEmpty()) {
        return;
    }

    _output_path = std::filesystem::path(dir_name.toStdString());
    ui->output_dir_label->setText(dir_name);
}

void Whisker_Widget::_changeWhiskerClip(int clip_dist)
{
    _clip_length = clip_dist;

    _traceButton();
}

void Whisker_Widget::_addDrawingCallback(std::string data_name)
{
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
std::vector<int> load_csv_lines_into_data_manager(DataManager* dm, std::string const & dir_name, std::string const & line_key)
{
    auto loaded_frames = std::vector<int>{};

    for (const auto & entry : std::filesystem::directory_iterator(dir_name))
    {
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
 * @brief read_hdf5_line_into_datamanager
 * @param dm
 * @param filename
 * @param line_key
 */
void read_hdf5_line_into_datamanager(DataManager* dm, std::string const  & filename, std::string const & line_key)
{
    auto frames =  read_array_hdf5(filename, "frames");
    auto y_coords = read_ragged_hdf5(filename, "x");
    auto x_coords = read_ragged_hdf5(filename, "y");

    auto line = dm->getData<LineData>(line_key);

    for (std::size_t i = 0; i < frames.size(); i ++) {
        line->addLineAtTime(frames[i], x_coords[i], y_coords[i]);
    }
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
        DataManager* dm,
        std::string const & whisker_group_name,
        int const num_whisker_to_track,
        int current_time,
        float similarity_threshold)
{

    std::vector<Line2D> whiskers = dm->getData<LineData>("unlabeled_whiskers")->getLinesAtTime(current_time);

    std::map<int,Line2D> previous_whiskers;
    for (std::size_t i = 0; i < static_cast<std::size_t>(num_whisker_to_track); i++) {

        std::string whisker_name = whisker_group_name + "_" + std::to_string(i);

        if (dm->getData<LineData>(whisker_name)) {
            auto whisker = dm->getData<LineData>(whisker_name)->getLinesAtTime(current_time - 1);
            if (whisker.size() > 0) {
                if (whisker[0].size() > 0) {
                    previous_whiskers[i] = whisker[0];
                }
            }
        }
    }

    std::map<int,bool> matched_previous;
    for (auto [key, prev_whisker] : previous_whiskers) {
        matched_previous[key] = false;
    }
    std::vector<int> assigned_ids(whiskers.size(), -1);

    for (std::size_t i = 0; i < whiskers.size(); ++i) {
        for (auto [prev_key, prev_whisker] : previous_whiskers) {
            if (matched_previous[prev_key]) continue;

            float similarity = whisker::fast_discrete_frechet_matrix(
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
            std::string whisker_name = whisker_group_name + "_" + std::to_string(assigned_ids[i]);
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
 * @brief _checkWhiskerNumMatchesExportNum
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
bool _checkWhiskerNumMatchesExportNum(DataManager* dm, int const num_whiskers_to_export, std::string const & whisker_group_name)
{

    auto current_time = dm->getTime()->getLastLoadedFrame();

    int whiskers_in_frame = 0;

    for (int i = 0; i< num_whiskers_to_export; i++)
    {
        std::string whisker_name = whisker_group_name + "_" + std::to_string(i);

        if (dm->getData<LineData>(whisker_name)) {
            auto whiskers = dm->getData<LineData>(whisker_name)->getLinesAtTime(current_time);
            if (whiskers.size() > 0) {
                if (whiskers[0].size() > 0) {
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
    DataManager* dm,
    std::vector<Line2D> & whiskers,
    std::string const & whisker_group_name,
    int const num_whisker_to_track,
    int current_time,
    float similarity_threshold)
{
    dm->getData<LineData>("unlabeled_whiskers")->clearLinesAtTime(current_time);

    for (auto & w: whiskers) {
        dm->getData<LineData>("unlabeled_whiskers")->addLineAtTime(current_time, w);
    }

    if (num_whisker_to_track > 0) {
        order_whiskers_by_position(dm, whisker_group_name,num_whisker_to_track, current_time, similarity_threshold);
    }
}

void clip_whisker(Line2D& line, int clip_length)
{
    if (clip_length <= 0 || clip_length > line.size()) {
        return; // Invalid clip length, do nothing
    }
    line.erase(line.end() - clip_length, line.end());
}

std::string generate_color() {
    std::stringstream ss;
    ss << "#";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    for (int i = 0; i < 3; ++i) {
        ss << std::setw(2) << std::setfill('0') << std::hex << dis(gen);
    }
    return ss.str();
}

std::string get_whisker_color(int whisker_index) {
    if (whisker_index >= whisker_colors.size()) {
        whisker_colors.push_back(generate_color());
    }
    return whisker_colors[whisker_index];
}
