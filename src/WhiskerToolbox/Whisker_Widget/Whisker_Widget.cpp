
#include "Whisker_Widget.h"
#include "qevent.h"

#include <QFileDialog>
#include <QElapsedTimer>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

#include "ui_Whisker_Widget.h"

Whisker_Widget::Whisker_Widget(Media_Window* scene, std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QWidget(parent),
    _wt{std::make_unique<WhiskerTracker>()},
    _scene{scene},
    _data_manager{data_manager},
    _selected_whisker{0},
    _selection_mode{Whisker_Select},
    _contact_start{0},
    _contact_epoch(false),
    ui(new Ui::Whisker_Widget)
{
    ui->setupUi(this);
    _data_manager->createLine("unlabeled_whiskers");
    _scene->addLineDataToScene("unlabeled_whiskers");
    _createActions(); 
};

Whisker_Widget::~Whisker_Widget()
{
    delete ui;
}

void Whisker_Widget::_createActions() {
    //connect(this,SIGNAL(this->show()),this,SLOT(_openActions()));
    //connect(this,SIGNAL(this->close()),this,SLOT(_closeActions()));
}

void Whisker_Widget::openWidget() {
    std::cout << "Whisker Widget Opened" << std::endl;

    connect(ui->trace_button,SIGNAL(clicked()),this,SLOT(_TraceButton()));
    connect(_scene,SIGNAL(leftClick(qreal,qreal)),this,SLOT(_ClickedInVideo(qreal,qreal)));

    connect(ui->save_image,SIGNAL(clicked()),this,SLOT(_SaveImageButton()));
    connect(ui->save_whisker_mask,SIGNAL(clicked()),this,SLOT(_SaveWhiskerMaskButton()));
    connect(ui->contact_button,SIGNAL(clicked()),this,SLOT(_ContactButton()));
    connect(ui->save_contact_button,SIGNAL(clicked()),this,SLOT(_SaveContact()));
    connect(ui->load_contact_button,SIGNAL(clicked()),this,SLOT(_LoadContact()));

    connect(ui->load_janelia_button,SIGNAL(clicked()),this,SLOT(_LoadJaneliaWhiskers()));


    if (_contact.empty()) {
        _contact = std::vector<Contact>(_data_manager->getTime()->getTotalFrameCount());
    }

    this->show();

}

void Whisker_Widget::_openActions() {
    std::cout << "Setting up actions for whisker tracker" << std::endl;
}

void Whisker_Widget::_closeActions() {
    std::cout << "Disconnecting actions for whisker tracker" << std::endl;
}


void Whisker_Widget::closeEvent(QCloseEvent *event) {
    std::cout << "Close event detected" << std::endl;

    disconnect(ui->trace_button,SIGNAL(clicked()),this,SLOT(_TraceButton()));
    disconnect(_scene,SIGNAL(leftClick(qreal,qreal)),this,SLOT(_ClickedInVideo(qreal,qreal)));

    disconnect(ui->save_image,SIGNAL(clicked()),this,SLOT(_SaveImageButton()));
    disconnect(ui->save_whisker_mask,SIGNAL(clicked()),this,SLOT(_SaveWhiskerMaskButton()));
    disconnect(ui->contact_button,SIGNAL(clicked()),this,SLOT(_ContactButton()));
    disconnect(ui->save_contact_button,SIGNAL(clicked()),this,SLOT(_SaveContact()));
    disconnect(ui->load_contact_button,SIGNAL(clicked()),this,SLOT(_LoadContact()));

    disconnect(ui->load_janelia_button,SIGNAL(clicked()),this,SLOT(_LoadJaneliaWhiskers()));
}

void Whisker_Widget::_TraceButton()
{
    QElapsedTimer timer2;
    timer2.start();

    auto media = _data_manager->getMediaData();
    
    _wt->trace(media->getRawData(),media->getHeight(), media->getWidth());

    //Add lines to data manager
    _addWhiskersToData();

    int t1 = timer2.elapsed();
    _DrawWhiskers();

    int t2 = timer2.elapsed();

    qDebug() << "The tracing took" << t1 << "ms and drawing took" << (t2-t1);
}

void Whisker_Widget::_SaveImageButton()
{
    
    auto media = _data_manager->getMediaData();
    
    auto data = media->getRawData();
    
    auto width = media->getWidth();
    auto height = media->getHeight();
    
    auto frame_id = _data_manager->getTime()->getLastLoadedFrame();

    QImage labeled_image(&data[0],width,height, QImage::Format_Grayscale8);

    std::stringstream ss;
    ss << std::setw(7) << std::setfill('0') << frame_id;

    std::string saveName = "img" +  ss.str() + ".png";
    std::cout << "Saving file " << saveName << std::endl;

    labeled_image.save(QString::fromStdString(saveName));

}

void Whisker_Widget::_SaveWhiskerMaskButton() {

    auto media = _data_manager->getMediaData();
    
    auto width = media->getWidth();
    auto height = media->getHeight();
    
    auto frame_id = _data_manager->getTime()->getLastLoadedFrame();

    QImage mask_image(width,height,QImage::Format_Grayscale8);

    mask_image.fill(Qt::black);
    
    if (!_wt->whiskers.empty()) {
        
        auto& w = _wt->whiskers[_selected_whisker-1];

        for (int i = 0; i < w.x.size(); i++) {

            auto x = std::lround(w.x[i]);
            auto y = std::lround(w.y[i]);

            mask_image.setPixelColor(x,y, Qt::white);
        }

    }

    std::stringstream ss;
    ss << std::setw(7) << std::setfill('0') << frame_id;

    std::string saveName = "w" +  ss.str() + ".png";
    std::cout << "Saving file " << saveName << std::endl;

    mask_image.save(QString::fromStdString(saveName));
}

void Whisker_Widget::_ContactButton() {
    
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

void Whisker_Widget::_SaveContact() {


    std::fstream fout;

    fout.open("contact.csv",std::fstream::out);

    for (auto& frame_contact : _contact) {
        if (frame_contact == Contact::Contact) {
            fout << "Contact" << "\n";
        } else {
            fout << "Nocontact" << "\n";
        }
    }

    fout.close();
}

void Whisker_Widget::_LoadContact() {

    auto contact_filename =  QFileDialog::getOpenFileName(
        this,
        "Load Video File",
        QDir::currentPath(),
        "All files (*.*) ;; MP4 (*.mp4)");


    std::ifstream fin(contact_filename.toStdString());

    std::string line;

    int row_num = 0;

    while (std::getline(fin,line)) {
        if (line == "Contact") {
            _contact[row_num] = Contact::Contact;
        }
        row_num++;
    }

    fin.close();
}

void Whisker_Widget::_addWhiskersToData()
{
    auto current_time = _data_manager->getTime()->getLastLoadedFrame();

    for (auto& w : _wt->whiskers) {

        _data_manager->getLine("unlabeled_whiskers")->addLineAtTime(current_time, w.x, w.y);

    }
}

void Whisker_Widget::_DrawWhiskers()
{
    _scene->UpdateCanvas();
    //_scene->clearLines(); // We should have the scene do this every time a frame is advanced
    //_data_manager->getLine("unlabeled_whiskers")->addLineAtTime(current_time, w.x, w.y);
    
    /*
    for (auto& w : _wt->whiskers) {

        auto whisker_color = (w.id == _selected_whisker) ? QPen(QColor(Qt::red)) : QPen(QColor(Qt::blue));
        
        _scene->addLine(w.x,w.y,whisker_color);

    }
    */
}

//x
void Whisker_Widget::_ClickedInVideo(qreal x_canvas,qreal y_canvas) {
    
    float x_media = x_canvas / _scene->getXAspect();
    float y_media = y_canvas / _scene->getYAspect();
    
    switch(_selection_mode) {
    case Whisker_Select: {
        std::tuple<float,int> nearest_whisker = _wt->get_nearest_whisker(x_media, y_media);
        if (std::get<0>(nearest_whisker) < 10.0f) {
            _selected_whisker = std::get<1>(nearest_whisker);
            _DrawWhiskers();
        }
        break;
    }
    case Whisker_Pad_Select:

        break;

    default:
        break;
    }
}

void Whisker_Widget::_LoadJaneliaWhiskers()
{
    auto janelia_name =  QFileDialog::getOpenFileName(
        this,
        "Load Whisker File",
        QDir::currentPath(),
        "All files (*.*) ;; whisker file (*.whiskers)");

    if (janelia_name.isNull()) {
        return;
    }

    auto whiskers_from_janelia = _wt->load_janelia_whiskers(janelia_name.toStdString());

    for (auto& [time, whiskers_in_frame] : whiskers_from_janelia)
    {
        for (auto& w : whiskers_in_frame) {
            _data_manager->getLine("unlabeled_whiskers")->addLineAtTime(time, w.x, w.y);
        }
    }
}
