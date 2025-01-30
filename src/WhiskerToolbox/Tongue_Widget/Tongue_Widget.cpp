
#include "Tongue_Widget.hpp"

#include "ui_Tongue_Widget.h"

#include "DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"

#include "Grabcut_Widget/Grabcut_Widget.hpp"
#include "Media_Window.hpp"
#include "TimeFrame.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"
#include "utils/opencv_utility.hpp"
#include "utils/string_manip.hpp"

#include <opencv2/opencv.hpp>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QPushButton>
#include "qevent.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>


const std::vector<std::string> tongue_colors = {
    "#00FF00", // greeeeeeeeen 
    "#8b008b", // dark magenta
    "#9b870c", // dark yellow
    "#00008b" // dark blue
};
Tongue_Widget::Tongue_Widget(Media_Window *scene, std::shared_ptr<DataManager> data_manager, TimeScrollBar* time_scrollbar, QWidget *parent) :
    QMainWindow(parent),
    _scene{scene},
    _data_manager{data_manager},
    _time_scrollbar{time_scrollbar},
    ui(new Ui::Tongue_Widget)
{
    ui->setupUi(this);

    connect(ui->load_img_btn, &QPushButton::clicked, this, &Tongue_Widget::_loadImgTongueMasks);

    connect(ui->begin_grabcut_btn, &QPushButton::clicked, this, &Tongue_Widget::_startGrabCut);
    connect(ui->savemasks_btn, &QPushButton::clicked, this, &Tongue_Widget::_exportMasks);
};

Tongue_Widget::~Tongue_Widget() {
    delete ui;
}

void Tongue_Widget::openWidget() {

    std::cout << "Tongue Widget Opened" << std::endl;

    this->show();
}

void Tongue_Widget::closeEvent(QCloseEvent *event) {
    std::cout << "Close event detected" << std::endl;

}

void Tongue_Widget::keyPressEvent(QKeyEvent *event) {

    //Data manager should be responsible for loading new value of data object
    //Main window can update displays with new data object position
    //Frame label is also updated.

    QMainWindow::keyPressEvent(event);

}

/**
 * @brief Tongue_Widget::_loadImgTongueMasks
 * Loads masks of tongue from image sequence
 */
void Tongue_Widget::_loadImgTongueMasks(){
    auto const dir_name =  QFileDialog::getExistingDirectory(
                              this,
                              "Load Tongue image files",
                              QDir::currentPath()).toStdString();
    if (dir_name.empty()) {
        return;
    }
    auto dir_path = std::filesystem::path(dir_name);

    auto mask_num = _data_manager->getKeys<MaskData>().size();
    auto mask_key = "Tongue_Mask" + std::to_string(mask_num);
    _data_manager->setData<MaskData>(mask_key);
    auto mask = _data_manager->getData<MaskData>(mask_key);

    auto media = _data_manager->getData<MediaData>("media");
    mask->setImageSize({media->getWidth(), media->getHeight()});

    for (const auto & img_it : std::filesystem::directory_iterator(dir_name))
    {

        auto img = load_mask_from_image(img_it.path().string(), true);

        auto img_mask = create_mask(img);

        auto const frame_num = remove_extension(extract_numbers_from_string(img_it.path().filename().string()));
        auto const frame_index = media->getFrameIndexFromNumber(std::stoi(frame_num));

        mask->addMaskAtTime(frame_index, img_mask);
        //std::cout << "Added " << x_coords.size() << " pts at frame " << frame_index << '\n';
    }

    _scene->addMaskDataToScene(mask_key);
    _scene->changeMaskColor(mask_key, tongue_colors[mask_num]);
}

void Tongue_Widget::_startGrabCut(){
    auto media = _data_manager->getData<MediaData>("media");
    auto const current_time = _data_manager->getTime()->getLastLoadedFrame();
    auto media_data = media->getProcessedData(current_time);


    bool is_gray = media->getFormat() == MediaData::DisplayFormat::Gray;
    cv::Mat img(
        media->getHeight(),
        media->getWidth(),
        is_gray ? CV_8UC1 : CV_8UC4,
        reinterpret_cast<cv::Scalar*>(media_data.data())
    );
    cv::cvtColor(img, img, is_gray ? cv::COLOR_GRAY2BGR : cv::COLOR_BGRA2BGR);

    if (!_grabcut_widget){
        _grabcut_widget = new Grabcut_Widget(_scene, _data_manager, _time_scrollbar);
    }
    auto frame = _data_manager->getTime()->getLastLoadedFrame();
    _grabcut_widget->setup(img, frame);
    drawn.push_back(frame);
    _grabcut_widget->openWidget();
}

void Tongue_Widget::_exportMasks() {
    auto const dir_name =  QFileDialog::getExistingDirectory(
                              this,
                              "Load Tongue image files",
                              QDir::currentPath()).toStdString();
    if (dir_name.empty()) {
        return;
    }
    auto dir_path = std::filesystem::path(dir_name);

    auto mask_name = "grabcut_masks";
    auto mask_data = _data_manager->getData<MaskData>(mask_name);
    auto media = _data_manager->getData<MediaData>("media");

    for (int i : drawn){
        auto mask = mask_data->getMasksAtTime(i)[0];
        QImage mask_img(mask_data->getImageSize().getWidth(), mask_data->getImageSize().getHeight(), QImage::Format_Grayscale8);
        mask_img.fill(0);
        for (auto [x, y] : mask){
            mask_img.setPixel(static_cast<int>(x), static_cast<int>(y), 0xFFFFFF);
        }
        std::string saveName = dir_path.string() + "/" + media->GetFrameID(i) + ".png";
        std::cout << "Saving file" << saveName << std::endl;

        mask_img.save(QString::fromStdString(saveName));
    }
}



