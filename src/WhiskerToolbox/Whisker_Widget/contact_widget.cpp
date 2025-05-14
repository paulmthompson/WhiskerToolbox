
#include "contact_widget.hpp"

#include "ui_contact_widget.h"

#include "DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series_Loader.hpp"
#include "DataManager/Media/Media_Data.hpp"

#include "TimeFrame.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"

#include <QElapsedTimer>
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>

#include <iostream>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <string>

Contact_Widget::Contact_Widget(std::shared_ptr<DataManager> data_manager, TimeScrollBar* time_scrollbar, QWidget *parent) :
    QWidget(parent),
      _data_manager{std::move(data_manager)},
    _time_scrollbar{time_scrollbar},
    _output_path{std::filesystem::current_path()},
    ui(new Ui::contact_widget)
{
    ui->setupUi(this);

    ui->output_dir_label->setText(QString::fromStdString(std::filesystem::current_path().string()));

    _contact_imgs = std::vector<QImage>();

    for (int i = 0; i < _image_buffer_size; i++) {
        _contact_imgs.emplace_back(130,130,QImage::Format_Grayscale8);
    }

    if (!_data_manager->getData<DigitalIntervalSeries>("Contact_Events")) {
        _data_manager->setData<DigitalIntervalSeries>("Contact_Events");
    } else {
        std::cout << "Contact Events already exist" << std::endl;
    }

    _data_manager->addCallbackToData("Contact_Events", [this]() {
        _calculateContactPeriods();
    });

    _scene = new QGraphicsScene();
    _scene->setSceneRect(0, 0, 650, 150);

    _createContactRectangles();
    _createContactPixmaps();

    ui->graphicsView->setScene(_scene);
    ui->graphicsView->show();

    ui->graphicsView->setTransformationAnchor(QGraphicsView::NoAnchor);

};

Contact_Widget::~Contact_Widget() {
    delete ui;
}

void Contact_Widget::openWidget() {

    connect(ui->contact_button, &QPushButton::clicked, this, &Contact_Widget::_contactButton);

    connect(ui->no_contact_button, &QPushButton::clicked, this, &Contact_Widget::_noContactButton);

    connect(ui->save_contact_button, &QPushButton::clicked, this, &Contact_Widget::_saveContactFrameByFrame);
    connect(ui->load_contact_button, &QPushButton::clicked, this, &Contact_Widget::_loadContact);
    connect(ui->pole_select, &QPushButton::clicked,this, &Contact_Widget::_poleSelectButton);

    connect(ui->bounding_box_size, &QSpinBox::valueChanged,this, &Contact_Widget::_setBoundingBoxWidth);

    connect(ui->flip_contact_button, &QPushButton::clicked, this, &Contact_Widget::_flipContactButton);

    connect(ui->output_dir_button, &QPushButton::clicked, this, &Contact_Widget::_changeOutputDir);

    connect(ui->contact_table, &QTableWidget::cellClicked, this, &Contact_Widget::_contactTableClicked);

    this->show();

}

void Contact_Widget::closeEvent(QCloseEvent *event) {

}

void Contact_Widget::setPolePos(float pole_x, float pole_y)
{
    if (_pole_select_mode) {
        _pole_pos = std::make_tuple(static_cast<int>(pole_x),static_cast<int>(pole_y));
        std::string const new_label = "(" + std::to_string(static_cast<int>(pole_x)) + ", " + std::to_string(static_cast<int>(pole_y)) + ")";
        ui->current_location_label->setText(QString::fromStdString(new_label));
        _pole_select_mode = false;
        updateFrame(_data_manager->getTime()->getLastLoadedFrame());
    }
}

void Contact_Widget::_poleSelectButton()
{
    _pole_select_mode = true;
}

void Contact_Widget::_setBoundingBoxWidth(int value)
{
    _bounding_box_width = value;
    updateFrame(_data_manager->getTime()->getLastLoadedFrame());
}

void Contact_Widget::updateFrame(int frame_id)
{

    QElapsedTimer timer2;
    timer2.start();

    auto _media = _data_manager->getData<MediaData>("media");

    auto const pole_x = static_cast<float>(std::get<0>(_pole_pos));
    auto const pole_y = static_cast<float>(std::get<1>(_pole_pos));

    for (int i = -2; i < 3; i++) {

        if ((frame_id + i < 0) | (frame_id + i > _data_manager->getTime()->getTotalFrameCount())) {
            continue;
        }

        auto media_data = _media->getProcessedData(frame_id + i);

        auto unscaled_image = QImage(&media_data[0],
                                 _media->getWidth(),
                                 _media->getHeight(),
                                 _getQImageFormat()
                                 );

        QRect const rect(static_cast<int>(pole_x) - _bounding_box_width/2,
                         static_cast<int>(pole_y) - _bounding_box_width/2,
                         _bounding_box_width,
                         _bounding_box_width);

        auto cropped_image = unscaled_image.copy(rect);

        _contact_imgs[i + 2] = cropped_image.scaled(130,130);

        _contact_pixmaps[i + 2]->setPixmap(QPixmap::fromImage(_contact_imgs[i + 2]));

    }

    auto contactIntervals = _data_manager->getData<DigitalIntervalSeries>("Contact_Events");

    if (contactIntervals->size() != 0) {
        _drawContactRectangles(frame_id);
    }

    _updateContactWidgets(frame_id);

    auto const t1 = timer2.elapsed();

    qDebug() << "Drawing 5 frames took " << t1;
}

void Contact_Widget::_updateContactWidgets(int frame_id)
{

    auto const contactIntervals = _data_manager->getData<DigitalIntervalSeries>("Contact_Events");

    if (contactIntervals->size() == 0)
    {
        return;
    }

    auto nearest_contact = find_closest_preceding_event(contactIntervals.get(), frame_id);

    if (nearest_contact != -1) {

        if (_highlighted_row != nearest_contact) {
            highlight_row(ui->contact_table, _highlighted_row, Qt::white);
            _highlighted_row = highlight_row(ui->contact_table, nearest_contact, Qt::yellow);
        }
    }
}

void Contact_Widget::_createContactPixmaps()
{
    const int image_width = 130;
    const int y_offset = 20;
    _contact_pixmaps = std::vector<QGraphicsPixmapItem*>();

    for (int i = 0; i < 5; i++) {

        _contact_pixmaps.push_back(new QGraphicsPixmapItem());
        _scene->addItem(_contact_pixmaps[i]);
        _contact_pixmaps[i]->setTransform(QTransform().translate(image_width * (i),y_offset),true);
    }

}

void Contact_Widget::_createContactRectangles()
{
    const int rect_width = 130;
    const int rect_height = 20;
    _contact_rectangle_items = std::vector<QGraphicsPathItem*>();

    for (int i = 0; i < 5; i++) {
        _contact_rectangle_items.push_back(new QGraphicsPathItem());

        QPainterPath contact_rectangle;
        contact_rectangle.addRect(0,0,rect_width,rect_height);
        contact_rectangle.setFillRule(Qt::WindingFill);

        _contact_rectangle_items[i] = _scene->addPath(contact_rectangle,QPen(Qt::green),QBrush(Qt::green));

        _contact_rectangle_items[i]->setTransform(QTransform().translate(rect_width * (i),0),true);

    }
}

void Contact_Widget::_drawContactRectangles(int frame_id) {

    auto contactIntervals = _data_manager->getData<DigitalIntervalSeries>("Contact_Events");

    for (int i = -2; i < 3; i++) {

        if ((frame_id + i < 0) | (frame_id + i > _data_manager->getTime()->getTotalFrameCount())) {
            continue;
        }

        if (contactIntervals->isEventAtTime(frame_id + i)) {
            _contact_rectangle_items[i + 2]->setPen(QPen(Qt::red));
            _contact_rectangle_items[i + 2]->setBrush(QBrush(Qt::red));
        } else {
            _contact_rectangle_items[i + 2]->setPen(QPen(Qt::green));
            _contact_rectangle_items[i + 2]->setBrush(QBrush(Qt::green));
        }
    }
}

QImage::Format Contact_Widget::_getQImageFormat() {

    auto _media = _data_manager->getData<MediaData>("media");
    switch(_media->getFormat())
    {
    case MediaData::DisplayFormat::Gray:
        return QImage::Format_Grayscale8;
    case MediaData::DisplayFormat::Color:
        return QImage::Format_RGBA8888;
    }
}

void Contact_Widget::_contactButton() {

    auto frame_num = _data_manager->getTime()->getLastLoadedFrame();
    auto contactIntervals = _data_manager->getData<DigitalIntervalSeries>("Contact_Events");

    // If we are in a contact epoch, we need to mark the termination frame and add those to block
    if (_contact_epoch) {

        _contact_epoch = false;

        ui->contact_button->setText("Mark Contact");

        contactIntervals->addEvent(_contact_start, frame_num);

    } else {
        // If we are not already in contact epoch, start one
        _contact_start = frame_num;

        _contact_epoch = true;

        ui->contact_button->setText("Mark Contact End");
    }
}

void Contact_Widget::_noContactButton()
{
    auto frame_num = _data_manager->getTime()->getLastLoadedFrame();
    auto contactIntervals = _data_manager->getData<DigitalIntervalSeries>("Contact_Events");

    // If we are in a contact epoch, we need to mark the termination frame and add those to block
    if (_contact_epoch) {

        _contact_epoch = false;

        ui->no_contact_button->setText("Mark No Contact");

        for (int i = _contact_start; i < frame_num; i++) {
            contactIntervals->setEventAtTime(i, false);
        }

    } else {
        // If we are not already in contact epoch, start one
        _contact_start = frame_num;

        _contact_epoch = true;

        ui->no_contact_button->setText("Mark No Contact End");
    }
}

void Contact_Widget::_saveContactFrameByFrame() {

    auto contactIntervals = _data_manager->getData<DigitalIntervalSeries>("Contact_Events");
    std::fstream fout;

    auto frame_by_frame_output = _output_path;

    fout.open(frame_by_frame_output.append("contact_FRAME_BY_FRAME.csv").string(), std::fstream::out);

    for (auto frame = 0; frame < _data_manager->getTime()->getTotalFrameCount(); frame++) {
        if (contactIntervals->isEventAtTime(frame)) {
            fout << "Contact" << "\n";
        } else {
            fout << "Nocontact" << "\n";
        }
    }

    fout.close();

    _saveContactBlocks();
}

void Contact_Widget::_saveContactBlocks() {

    auto filename = "contact_BLOCKS.csv";
    auto key = "Contact_Events";

    auto block_output = _output_path;
    block_output.append(filename);

    auto contactEvents = _data_manager->getData<DigitalIntervalSeries>(key)->getDigitalIntervalSeries();

    save_intervals(contactEvents, block_output.string());

}

void Contact_Widget::_loadContact() {

    auto contact_filename = QFileDialog::getOpenFileName(
        this,
        "Load Video File",
        QDir::currentPath(),
        "All files (*.*) ;; MP4 (*.mp4)");


    std::ifstream fin(contact_filename.toStdString());

    std::string line;

    int row_num = 0;

    std::vector<uint8_t> event(_data_manager->getTime()->getTotalFrameCount());

    while (std::getline(fin, line)) {
        if ((line == "Contact") || (line == "Contact\r")) {
            event[row_num] = 1;
        }
        row_num++;
    }

    _data_manager->getData<DigitalIntervalSeries>("Contact_Events")->createIntervalsFromBool(event);

    fin.close();
}

void Contact_Widget::_buildContactTable()
{
    auto contactEvents = _data_manager->getData<DigitalIntervalSeries>("Contact_Events")->getDigitalIntervalSeries();

    ui->contact_table->setRowCount(0);
    for (int i=0; i < contactEvents.size(); i++)
    {
        ui->contact_table->insertRow(ui->contact_table->rowCount());
        ui->contact_table->setItem(i,0,new QTableWidgetItem(QString::number(std::round(contactEvents[i].start))));
        ui->contact_table->setItem(i,1,new QTableWidgetItem(QString::number(std::round(contactEvents[i].end))));

    }

    _highlighted_row = -1;
    auto frame_id = _data_manager->getTime()->getLastLoadedFrame();
    _updateContactWidgets(frame_id);
}

void Contact_Widget::_calculateContactPeriods()
{

    auto contactEvents = _data_manager->getData<DigitalIntervalSeries>("Contact_Events");
    ui->total_contact_label->setText(QString::number(contactEvents->size()));

    _buildContactTable();
}

void Contact_Widget::_flipContactButton()
{

    auto frame_num = _data_manager->getTime()->getLastLoadedFrame();
    auto contact = _data_manager->getData<DigitalIntervalSeries>("Contact_Events");

    if (contact->isEventAtTime(frame_num)) {
        contact->setEventAtTime(frame_num, false);
    } else {
        contact->setEventAtTime(frame_num, true);
    }
    _drawContactRectangles(frame_num);
}

void Contact_Widget::_changeOutputDir()
{
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

void Contact_Widget::_contactTableClicked(int row, int column) {
    if (column == 0 || column == 1) {
        int const frame_id = ui->contact_table->item(row, column)->text().toInt();
        _time_scrollbar->changeScrollBarValue(frame_id);
    }
}

int highlight_row(QTableWidget* table, int row_index, Qt::GlobalColor color) {
    if (row_index < 0 || row_index >= table->rowCount()) {
        return -1; // Invalid row index, do nothing
    }

    for (int col = 0; col < table->columnCount(); ++col) {
        table->item(row_index, col)->setBackground(color); // Set the highlight color
    }
    return row_index;
}
