#include "Grabcut_Widget.hpp"
#include "ui_Grabcut_Widget.h"
#include "utils/opencv_utility.hpp"

#include <opencv2/imgcodecs.hpp>
#include <QMouseEvent>
#include <QComboBox>

#include <iostream>

Grabcut_Widget::Grabcut_Widget(Media_Window *scene, std::shared_ptr<DataManager> data_manager, TimeScrollBar* time_scrollbar, QWidget *parent) :
    QMainWindow(parent),
    _scene{scene},
    _data_manager{data_manager},
    _time_scrollbar{time_scrollbar},
    ui(new Ui::Grabcut_Widget)
{

    ui->setupUi(this);
    setMouseTracking(true);
    // https://stackoverflow.com/questions/63804221/mouse-tracking-does-not-work-in-the-widget-in-the-main-widget
    ui->editor_label->setAttribute(Qt::WA_TransparentForMouseEvents);

    connect(ui->iter_pushbtn, &QPushButton::clicked, this, &Grabcut_Widget::_grabcutIter);
    connect(ui->thickness_spinbox, &QSpinBox::valueChanged, this, &Grabcut_Widget::_setBrushThickness);
    connect(ui->color_combobox, &QComboBox::currentIndexChanged, this, &Grabcut_Widget::_setColor);
    connect(ui->reset_pushbtn, &QPushButton::clicked, this, &Grabcut_Widget::_reset);
    connect(ui->done_pushbtn, &QPushButton::clicked, this, &Grabcut_Widget::_saveMask);
    connect(ui->transparency_slider, &QSlider::valueChanged, this, &Grabcut_Widget::_setMaskTransparency);
    connect(ui->exit_pushbtn, &QPushButton::clicked, this, [this](){
        this->close();
    });
}

void Grabcut_Widget::setup(cv::Mat img, int frame_index) {
    _img = img;
    _height = img.rows;
    _width = img.cols;
    _frame_index = frame_index;
    _tool = GrabCutTool(img);
    _updateDisplays();
    ui->editor_label->setPixmap(_img_disp_pixmap);
    ui->editor_label->setScaledContents(true);
    _reset();
}

void Grabcut_Widget::_grabcutIter(){
    _tool.grabcutIter();
    _updateDisplays();
}

void Grabcut_Widget::_setBrushThickness(){
    _tool.setBrushThickness(ui->thickness_spinbox->value());
    _updateDisplays();
}

void Grabcut_Widget::_setColor(){
    switch (ui->color_combobox->currentIndex()){
    case 0:
        _tool.setColor(cv::GC_BGD);
        break;
    case 1:
        _tool.setColor(cv::GC_FGD);
        break;
    case 2:
        _tool.setColor(cv::GC_PR_BGD);
        break;
    case 3:
        _tool.setColor(cv::GC_PR_FGD);
        break;
    default:
        std::cout << "Mysterious error\n";
        break;
    }
}

void Grabcut_Widget::_reset(){
    _tool.reset();
    _updateDisplays();
    ui->thickness_spinbox->setValue(5);
    ui->color_combobox->setCurrentIndex(0);
    ui->transparency_slider->setValue(0);
}

Grabcut_Widget::~Grabcut_Widget() {
    delete ui;
}

void Grabcut_Widget::openWidget() {
    std::cout << "Grabcut widget Opened" << std::endl;

    this->show();
}

void Grabcut_Widget::closeEvent(QCloseEvent *event) {
    std::cout << "Close event detected" << std::endl;

}

void Grabcut_Widget::keyPressEvent(QKeyEvent *event) {

    //Data manager should be responsible for loading new value of data object
    //Main window can update displays with new data object position
    //Frame label is also updated.

    QMainWindow::keyPressEvent(event);

}

QPoint Grabcut_Widget::_scaleTransMouse(QPoint p){
    p -= ui->editor_label->pos();
    p.setX(p.x() * _width / ui->editor_label->width());
    p.setY(p.y() * _height / ui->editor_label->height());
    return p;
}

void Grabcut_Widget::mousePressEvent(QMouseEvent *event){
    if (event->button() == Qt::LeftButton) {
        QPoint p = _scaleTransMouse(event->pos());
        _tool.mouseDown(p.x(), p.y());
        _updateDisplays();
    }
}

void Grabcut_Widget::mouseMoveEvent(QMouseEvent *event){
    if (ui->editor_label->rect().contains(event->pos())){
        if (_tool.getRectStage()){
            setCursor(Qt::CrossCursor);
        } else {
            setCursor(Qt::BlankCursor);
        }
    } else {
        setCursor(Qt::ArrowCursor);
    }

    QPoint p = _scaleTransMouse(event->pos());
    _tool.mouseMove(p.x(), p.y());
    _updateDisplays();
}

void Grabcut_Widget::mouseReleaseEvent(QMouseEvent *event){
    if (event->button() == Qt::LeftButton) {
        QPoint p = _scaleTransMouse(event->pos());
        _tool.mouseUp(p.x(), p.y());
        _updateDisplays();
    }
}

void Grabcut_Widget::_updateDisplays(){
    _img_disp_pixmap = QPixmap::fromImage(QImage(reinterpret_cast<unsigned char*>(_tool.getDisp().data), _width, _height, QImage::Format_RGB888));
    ui->editor_label->setPixmap(_img_disp_pixmap);
    ui->editor_label->repaint();
}

void Grabcut_Widget::_saveMask(){
    const char* mask_name = "grabcut_masks";
    if (!_data_manager->getMask(mask_name)) {
        std::cout << "Creating " << mask_name << " in data manager " << std::endl;
        _data_manager->createMask(mask_name);
        _scene->addMaskDataToScene(mask_name);
        _scene->addMaskColor(mask_name, QColor(252, 169, 35));
    }

    cv::Mat mask = _tool.getMask();
    for (int i=0; i<mask.rows; ++i){
        for (int j=0; j<mask.cols; ++j){
            if (mask.at<uint8_t>(i, j) == cv::GC_PR_FGD || mask.at<uint8_t>(i, j) == cv::GC_FGD){
                mask.at<uint8_t>(i, j) = 0;
            } else {
                mask.at<uint8_t>(i, j) = 255;
            }
        }
    }
    auto pts = create_mask(mask);
    auto mask_data = _data_manager->getMask(mask_name);
    mask_data->setMaskHeight(_height);
    mask_data->setMaskWidth(_width);

    mask_data->clearMasksAtTime(_frame_index);
    mask_data->addMaskAtTime(_frame_index, pts);
    this->close();
}

void Grabcut_Widget::_setMaskTransparency(){
    _tool.setMaskDispTransparency(ui->transparency_slider->value());
    _updateDisplays();
}
