
#include "Whisker_Widget.h"
#include "qevent.h"

#include <QFileDialog>
#include <QElapsedTimer>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <filesystem>

#include "ui_Whisker_Widget.h"

Whisker_Widget::Whisker_Widget(Media_Window *scene, std::shared_ptr<DataManager> data_manager, QWidget *parent) :
        QWidget(parent),
        _wt{std::make_unique<WhiskerTracker>()},
        _scene{scene},
        _data_manager{data_manager},
        _selected_whisker{0},
        _selection_mode{Whisker_Select},
        _contact_start{0},
        _contact_epoch(false),
        _length_threshold{75.0},
        _whisker_pad{0.0, 0.0},
        _face_orientation{Facing_Top},
        _num_whisker_to_track{0},
        ui(new Ui::Whisker_Widget) {
    ui->setupUi(this);
    _data_manager->createLine("unlabeled_whiskers");
    _scene->addLineDataToScene("unlabeled_whiskers");

};

Whisker_Widget::~Whisker_Widget() {
    delete ui;
}

void Whisker_Widget::openWidget() {
    std::cout << "Whisker Widget Opened" << std::endl;

    connect(ui->trace_button, SIGNAL(clicked()), this, SLOT(_traceButton()));
    connect(_scene, SIGNAL(leftClick(qreal, qreal)), this, SLOT(_clickedInVideo(qreal, qreal)));

    connect(ui->save_image, SIGNAL(clicked()), this, SLOT(_saveImageButton()));
    connect(ui->save_whisker_mask, SIGNAL(clicked()), this, SLOT(_saveWhiskerMaskButton()));
    connect(ui->contact_button, SIGNAL(clicked()), this, SLOT(_contactButton()));
    connect(ui->save_contact_button, SIGNAL(clicked()), this, SLOT(_saveContact()));
    connect(ui->load_contact_button, SIGNAL(clicked()), this, SLOT(_loadContact()));

    connect(ui->load_janelia_button, SIGNAL(clicked()), this, SLOT(_loadJaneliaWhiskers()));
    connect(ui->whisker_pad_select, SIGNAL(clicked()), this, SLOT(_selectWhiskerPad()));
    connect(ui->length_threshold_spinbox, SIGNAL(valueChanged(double)), this,
            SLOT(_changeWhiskerLengthThreshold(double)));

    connect(ui->face_orientation, SIGNAL(currentIndexChanged(int)), this, SLOT(_selectFaceOrientation(int)));
    connect(ui->whisker_number, SIGNAL(valueChanged(int)), this, SLOT(_selectNumWhiskersToTrack(int)));
    connect(ui->export_image_csv,SIGNAL(clicked()),this, SLOT(_exportImageCSV()));


    if (_contact.empty()) {
        _contact = std::vector<Contact>(_data_manager->getTime()->getTotalFrameCount());
    }

    this->show();

}

void Whisker_Widget::closeEvent(QCloseEvent *event) {
    std::cout << "Close event detected" << std::endl;

    disconnect(ui->trace_button, SIGNAL(clicked()), this, SLOT(_traceButton()));
    disconnect(_scene, SIGNAL(leftClick(qreal, qreal)), this, SLOT(_clickedInVideo(qreal, qreal)));

    disconnect(ui->save_image, SIGNAL(clicked()), this, SLOT(_saveImageButton()));
    disconnect(ui->save_whisker_mask, SIGNAL(clicked()), this, SLOT(_saveWhiskerMaskButton()));
    disconnect(ui->contact_button, SIGNAL(clicked()), this, SLOT(_contactButton()));
    disconnect(ui->save_contact_button, SIGNAL(clicked()), this, SLOT(_saveContact()));
    disconnect(ui->load_contact_button, SIGNAL(clicked()), this, SLOT(_loadContact()));

    disconnect(ui->load_janelia_button, SIGNAL(clicked()), this, SLOT(_loadJaneliaWhiskers()));
    disconnect(ui->whisker_pad_select, SIGNAL(clicked()), this, SLOT(_selectWhiskerPad()));

    disconnect(ui->length_threshold_spinbox, SIGNAL(valueChanged(double)), this,
               SLOT(_changeWhiskerLengthThreshold(double)));

    disconnect(ui->face_orientation, SIGNAL(currentIndexChanged(int)), this, SLOT(_selectFaceOrientation(int)));
    disconnect(ui->whisker_number, SIGNAL(valueChanged(int)), this, SLOT(_selectNumWhiskersToTrack(int)));

    disconnect(ui->export_image_csv,SIGNAL(clicked()),this, SLOT(_exportImageCSV()));
}

void Whisker_Widget::_traceButton() {
    QElapsedTimer timer2;
    timer2.start();

    auto media = _data_manager->getMediaData();

    _wt->trace(media->getRawData(), media->getHeight(), media->getWidth());


    //Add lines to data manager
    _addWhiskersToData();

    int t1 = timer2.elapsed();
    _drawWhiskers();

    int t2 = timer2.elapsed();

    qDebug() << "The tracing took" << t1 << "ms and drawing took" << (t2 - t1);
}

void Whisker_Widget::_saveImageButton() {
    _saveImage("./");
}

void Whisker_Widget::_saveImage(const std::string folder)
{
    auto media = _data_manager->getMediaData();

    auto data = media->getRawData();

    auto width = media->getWidth();
    auto height = media->getHeight();

    auto frame_id = _data_manager->getTime()->getLastLoadedFrame();

    QImage labeled_image(&data[0], width, height, QImage::Format_Grayscale8);

    std::stringstream ss;
    ss << std::setw(7) << std::setfill('0') << frame_id;

    std::string saveName = "img" + ss.str() + ".png";
    std::cout << "Saving file " << saveName << std::endl;

    labeled_image.save(QString::fromStdString(folder + saveName));
}

void Whisker_Widget::_saveWhiskerMaskButton() {

    auto media = _data_manager->getMediaData();

    auto width = media->getWidth();
    auto height = media->getHeight();

    auto frame_id = _data_manager->getTime()->getLastLoadedFrame();

    QImage mask_image(width, height, QImage::Format_Grayscale8);

    mask_image.fill(Qt::black);

    if (!_wt->whiskers.empty()) {

        auto &w = _wt->whiskers[_selected_whisker - 1];

        for (int i = 0; i < w.x.size(); i++) {

            auto x = std::lround(w.x[i]);
            auto y = std::lround(w.y[i]);

            mask_image.setPixelColor(x, y, Qt::white);
        }

    }

    std::stringstream ss;
    ss << std::setw(7) << std::setfill('0') << frame_id;

    std::string saveName = "w" + ss.str() + ".png";
    std::cout << "Saving file " << saveName << std::endl;

    mask_image.save(QString::fromStdString(saveName));
}

void Whisker_Widget::_contactButton() {

    auto frame_num = _data_manager->getTime()->getLastLoadedFrame();

    // If we are in a contact epoch, we need to mark the termination frame and add those to block
    if (_contact_epoch) {

        _contact_epoch = false;

        ui->contact_button->setText("Mark Contact");

        for (int i = _contact_start; i < frame_num; i++) {
            _contact[i] = Contact::Contact;
        }

    } else {
        // If we are not already in contact epoch, start one
        _contact_start = frame_num;

        _contact_epoch = true;

        ui->contact_button->setText("Mark Contact End");
    }
}

void Whisker_Widget::_saveContact() {


    std::fstream fout;

    fout.open("contact.csv", std::fstream::out);

    for (auto &frame_contact: _contact) {
        if (frame_contact == Contact::Contact) {
            fout << "Contact" << "\n";
        } else {
            fout << "Nocontact" << "\n";
        }
    }

    fout.close();
}

void Whisker_Widget::_loadContact() {

    auto contact_filename = QFileDialog::getOpenFileName(
            this,
            "Load Video File",
            QDir::currentPath(),
            "All files (*.*) ;; MP4 (*.mp4)");


    std::ifstream fin(contact_filename.toStdString());

    std::string line;

    int row_num = 0;

    while (std::getline(fin, line)) {
        if (line == "Contact") {
            _contact[row_num] = Contact::Contact;
        }
        row_num++;
    }

    fin.close();
}

void Whisker_Widget::_selectWhiskerPad() {
    _selection_mode = Selection_Type::Whisker_Pad_Select;
}


/**
 *
 *
 *
 * @brief Whisker_Widget::_addWhiskersToData
 */
void Whisker_Widget::_addWhiskersToData() {

    auto current_time = _data_manager->getTime()->getLastLoadedFrame();
    _data_manager->getLine("unlabeled_whiskers")->clearLinesAtTime(current_time);

    for (auto &w: _wt->whiskers) {
        auto w_length = _wt->calculateWhiskerLength(w);

        if (w_length < _length_threshold) {
            continue;
        }
        _wt->alignWhiskerToFollicle(w, std::get<0>(_whisker_pad), std::get<1>(_whisker_pad));

        _data_manager->getLine("unlabeled_whiskers")->addLineAtTime(current_time, w.x, w.y);
    }

    if (_num_whisker_to_track > 0) {
        _orderWhiskersByPosition();
    }
}

void Whisker_Widget::_drawWhiskers() {
    _scene->UpdateCanvas();
}

void Whisker_Widget::_changeWhiskerLengthThreshold(double new_threshold) {
    _length_threshold = static_cast<float>(new_threshold);
}

//x
void Whisker_Widget::_clickedInVideo(qreal x_canvas, qreal y_canvas) {

    float x_media = x_canvas / _scene->getXAspect();
    float y_media = y_canvas / _scene->getYAspect();

    switch (_selection_mode) {
        case Whisker_Select: {
            std::tuple<float, int> nearest_whisker = _wt->get_nearest_whisker(x_media, y_media);
            if (std::get<0>(nearest_whisker) < 10.0f) {
                _selected_whisker = std::get<1>(nearest_whisker);
                _drawWhiskers();
            }
            break;
        }
        case Whisker_Pad_Select: {
            _whisker_pad = std::make_tuple(static_cast<int>(x_media), static_cast<int>(y_media));
            std::string whisker_pad_label =
                    "(" + std::to_string(static_cast<int>(x_media)) + "," + std::to_string(static_cast<int>(y_media)) +
                    ")";
            ui->whisker_pad_pos_label->setText(QString::fromStdString(whisker_pad_label));
            _selection_mode = Whisker_Select;
            break;
        }
        default:
            break;
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

    auto whiskers_from_janelia = _wt->load_janelia_whiskers(janelia_name.toStdString());

    for (auto &[time, whiskers_in_frame]: whiskers_from_janelia) {
        for (auto &w: whiskers_in_frame) {

            auto w_length = _wt->calculateWhiskerLength(w);
            if (w_length > _length_threshold) {
                _data_manager->getLine("unlabeled_whiskers")->addLineAtTime(time, w.x, w.y);
            }
        }
    }
}

void Whisker_Widget::_selectFaceOrientation(int index) {
    if (index == 0) {
        _face_orientation = Face_Orientation::Facing_Top;
    } else if (index == 1) {
        _face_orientation = Face_Orientation::Facing_Bottom;
    } else if (index == 2) {
        _face_orientation = Face_Orientation::Facing_Left;
    } else {
        _face_orientation = Face_Orientation::Facing_Right;
    }
}

void Whisker_Widget::_selectNumWhiskersToTrack(int n_whiskers) {
    _num_whisker_to_track = n_whiskers;

    if (n_whiskers == 0) {
        return;
    }

    std::string whisker_name = "whisker_" + std::to_string(n_whiskers);

    if (!_data_manager->getLine(whisker_name)) {
        std::cout << "Creating " << whisker_name << std::endl;
        _data_manager->createLine(whisker_name);
        _scene->addLineDataToScene(whisker_name);
    }
}

/**
 * @brief Whisker_Widget::_orderWhiskersByPosition
 *
 * (0,0) coordinate is the top left of the video. Here we arrange the whiskers
 * such that the most posterior whisker is given identity of 1, next most posterior is
 * 2, etc.
 *
 */
void Whisker_Widget::_orderWhiskersByPosition() {
    auto base_positions = _getWhiskerBasePositions();

    std::vector<int> base_position_order = std::vector<int>(base_positions.size());
    std::iota(base_position_order.begin(), base_position_order.end(), 0);

    if (_face_orientation == Facing_Top) {
        // Facing toward 0 y, so larger Y is more posterior
        std::sort(std::begin(base_position_order),
                  std::end(base_position_order),
                  [&](int i1, int i2) { return base_positions[i1].y > base_positions[i2].y; });
    } else if (_face_orientation == Facing_Bottom) {
        // Facing toward maximum y, so smaller y is more posterior
        std::sort(std::begin(base_position_order),
                  std::end(base_position_order),
                  [&](int i1, int i2) { return base_positions[i1].y < base_positions[i2].y; });
    } else if (_face_orientation == Facing_Left) {
        // Facing toward 0 x, so larger x is more posterior
        std::sort(std::begin(base_position_order),
                  std::end(base_position_order),
                  [&](int i1, int i2) { return base_positions[i1].x > base_positions[i2].x; });
    } else {
        // Facing toward maximum x, so smaller x is more posterior
        std::sort(std::begin(base_position_order),
                  std::end(base_position_order),
                  [&](int i1, int i2) { return base_positions[i1].x < base_positions[i2].x; });
    }

    auto current_time = _data_manager->getTime()->getLastLoadedFrame();
    auto whiskers = _data_manager->getLine("unlabeled_whiskers")->getLinesAtTime(current_time);

    for (int i = 0; i < _num_whisker_to_track; i++) {
        std::cout << "The " << i << " position whisker is " << base_position_order[i];
        std::cout << " with follicle at " << "(" << base_positions[base_position_order[i]].x << ","
                  << base_positions[base_position_order[i]].y << ")" << std::endl;

        std::string whisker_name = "whisker_" + std::to_string(i + 1);

        _data_manager->getLine(whisker_name)->addLineAtTime(current_time, whiskers[base_position_order[i]]);
    }
}

void _printBasePositionOrder(std::vector<Point2D> &base_positions) {
    std::cout << "The order of whisker base positions: " << std::endl;

    for (int i = 0; i < base_positions.size(); i++) {
        std::cout << "Whisker " << i << " at " << "(" << base_positions[i].x << "," << base_positions[i].y << ")"
                  << std::endl;
    }
}

std::vector<Point2D> Whisker_Widget::_getWhiskerBasePositions() {
    auto base_positions = std::vector<Point2D>{};
    auto current_time = _data_manager->getTime()->getLastLoadedFrame();

    auto whiskers = _data_manager->getLine("unlabeled_whiskers")->getLinesAtTime(current_time);

    for (auto &whisker: whiskers) {
        base_positions.push_back(whisker[0]);
    }

    //_printBasePositionOrder(base_positions);

    return base_positions;
}

void Whisker_Widget::_exportImageCSV()
{

    std::string folder = "./images/";

    std::filesystem::create_directory(folder);
    _saveImage(folder);

    auto current_time = _data_manager->getTime()->getLastLoadedFrame();

    for (int i = 0; i<_num_whisker_to_track; i++)
    {
        std::string whisker_name = "whisker_" + std::to_string(i + 1);

        auto whiskers = _data_manager->getLine(whisker_name)->getLinesAtTime(current_time);

        std::string folder = "./data/" + whisker_name + "/";
        std::filesystem::create_directory(folder);

        _saveWhiskerAsCSV(folder, whiskers[0]);
    }
}

void Whisker_Widget::_saveWhiskerAsCSV(const std::string& folder, const std::vector<Point2D>& whisker)
{
    auto frame_id = _data_manager->getTime()->getLastLoadedFrame();

    std::stringstream ss;
    ss << std::setw(7) << std::setfill('0') << frame_id;

    std::string saveName = ss.str() + ".csv";

    std::fstream myfile;
    myfile.open (folder + saveName, std::fstream::out);

    //myfile << std::fixed << std::setprecision(2);
    for (auto& point: whisker)
    {
        myfile << point.x << "," << point.y << "\n";
    }

    myfile.close();

}
