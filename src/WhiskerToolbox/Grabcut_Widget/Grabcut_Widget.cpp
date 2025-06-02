#include "Grabcut_Widget.hpp"
#include "ui_Grabcut_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "utils/opencv_utility.hpp"

#include <opencv2/imgcodecs.hpp>
#include <QMouseEvent>
#include <QComboBox>

#include <iostream>

/**
 * @brief Constructor for Grabcut Widget.
 * @param scene
 * @param data_manager
 * @param parent
 */
Grabcut_Widget::Grabcut_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QMainWindow(parent),
    _data_manager{std::move(data_manager)},
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

/**
 * @brief Loads the image to edit and frame number for saving purposes into the widget
 * @param img Image data as cv::Mat, must be of pixel type CV_8UC3
 * @param frame_index Frame index number which the mask will be saved to when requested
 */
void Grabcut_Widget::setup(cv::Mat img, int frame_index) {
    _img = std::move(img);
    _height = _img.rows;
    _width = _img.cols;
    _frame_index = frame_index;
    _tool = GrabCutTool(_img);
    _updateDisplays();
    ui->editor_label->setPixmap(_img_disp_pixmap);
    ui->editor_label->setScaledContents(true);
    ui->iter_pushbtn->setEnabled(false);
    _reset();
}

/**
 * @brief Runs one interation of grabCut and updates the display
 */
void Grabcut_Widget::_grabcutIter(){
    _tool.grabcutIter();
    _updateDisplays();
}

/**
 * @brief Sets brush thickness to the value in the thickness spinbox in the widget
 */
void Grabcut_Widget::_setBrushThickness(){
    _tool.setBrushThickness(ui->thickness_spinbox->value());
    _updateDisplays();
}

/**
 * @brief Sets color according to the value in the dropbox in the widget
 */
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

/**
 * @brief Resets the tool and UI to the intial state
 */
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

/**
 * @brief Transforms a coordinate of the mouse into the local coordinates of the image
 * @param p QPoint that holds the mouse position on the widget
 * @return
 */
QPoint Grabcut_Widget::_scaleTransMouse(QPoint p){
    p -= ui->editor_label->pos();
    p.setX(p.x() * _width / ui->editor_label->width());
    p.setY(p.y() * _height / ui->editor_label->height());
    return p;
}

/**
 * @brief Mouse press event handler
 * @param event Mouse event
 */
void Grabcut_Widget::mousePressEvent(QMouseEvent *event){
    if (event->button() == Qt::LeftButton) {
        QPoint const p = _scaleTransMouse(event->pos());
        _tool.mouseDown(p.x(), p.y());
        _updateDisplays();
    }
}

/**
 * @brief Mouse move event handler
 * @param event Mouse event
 */
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

    QPoint const p = _scaleTransMouse(event->pos());
    _tool.mouseMove(p.x(), p.y());
    _updateDisplays();
}

/**
 * @brief Mouse release event handler
 * @param event Mouse event
 */
void Grabcut_Widget::mouseReleaseEvent(QMouseEvent *event){
    if (event->button() == Qt::LeftButton) {
        QPoint const p = _scaleTransMouse(event->pos());
        _tool.mouseUp(p.x(), p.y());
        _updateDisplays();
        if (!_tool.getRectStage()){
            ui->iter_pushbtn->setEnabled(true);
        }
    }
}

/**
 * @brief Updates display
 */
void Grabcut_Widget::_updateDisplays(){
    _img_disp_pixmap = QPixmap::fromImage(QImage(reinterpret_cast<unsigned char*>(_tool.getDisp().data), _width, _height, QImage::Format_RGB888));
    ui->editor_label->setPixmap(_img_disp_pixmap);
    ui->editor_label->repaint();
}

/**
 * @brief Saves the created mask to the data manager, replacing previously drawn masks
 */
void Grabcut_Widget::_saveMask(){
    const char* mask_name = "grabcut_masks";
    if (!_data_manager->getData<MaskData>(mask_name)) {
        std::cout << "Creating " << mask_name << " in data manager " << std::endl;
        _data_manager->setData<MaskData>(mask_name);
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
    auto mask_data = _data_manager->getData<MaskData>(mask_name);
    mask_data->setImageSize({_width,_height});

    mask_data->clearAtTime(_frame_index);
    mask_data->addAtTime(_frame_index, pts);
    this->close();
}

/**
 * @brief Sets the transparency of the mask overlay 
 */
void Grabcut_Widget::_setMaskTransparency(){
    _tool.setMaskDispTransparency(static_cast<float>(ui->transparency_slider->value()) / 100);
    _updateDisplays();
}
