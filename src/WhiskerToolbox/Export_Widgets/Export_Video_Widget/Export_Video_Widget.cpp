
#include "Export_Video_Widget.hpp"

#include "ui_Export_Video_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"

#include "Media_Window/Media_Window.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"

#include "opencv2/opencv.hpp"
#include <QFileDialog>
#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QCoreApplication>

#include <filesystem>
#include <iostream>
#include <regex>

Export_Video_Widget::Export_Video_Widget(
        std::shared_ptr<DataManager> data_manager,
        Media_Window * scene,
        TimeScrollBar * time_scrollbar,
        QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Export_Video_Widget),
      _data_manager{std::move(data_manager)},
      _scene{scene},
      _time_scrollbar{time_scrollbar} {
    ui->setupUi(this);

    connect(ui->export_video_button, &QPushButton::clicked, this, &Export_Video_Widget::_exportVideo);
    
    // Connect title sequence controls for live preview updates
    connect(ui->title_text_edit, &QTextEdit::textChanged, this, &Export_Video_Widget::_updateTitlePreview);
    connect(ui->font_size_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), this, &Export_Video_Widget::_updateTitlePreview);
    connect(ui->title_sequence_groupbox, &QGroupBox::toggled, this, &Export_Video_Widget::_updateTitlePreview);

    ui->start_frame_spinbox->setMaximum(_data_manager->getTime()->getTotalFrameCount());
    ui->end_frame_spinbox->setMaximum(_data_manager->getTime()->getTotalFrameCount());

    _video_writer = std::make_unique<cv::VideoWriter>();
    
    // Initialize title preview
    _updateTitlePreview();
}

Export_Video_Widget::~Export_Video_Widget() {
    delete ui;
}

void Export_Video_Widget::openWidget() {
    this->show();
}

void Export_Video_Widget::_exportVideo() {

    auto start_num = ui->start_frame_spinbox->value();

    auto end_num = ui->end_frame_spinbox->value();

    if (end_num == -1) {
        end_num = _data_manager->getTime()->getTotalFrameCount();
    }

    if (start_num >= end_num) {
        std::cout << "Start frame must be less than end frame" << std::endl;
    }

    auto filename = ui->output_filename->text().toStdString();

    // If filename doens't have .mp4 as ending, add it
    if (!std::regex_match(filename, std::regex(".*\\.mp4"))) {
        filename += ".mp4";
    }

    // Get a sample frame first to determine the exact canvas dimensions
    // This ensures title frames match the actual video frame size
    _time_scrollbar->changeScrollBarValue(start_num);
    
    // Force canvas update and wait briefly for it to process
    QCoreApplication::processEvents();
    
    // Get the actual canvas image to determine real dimensions
    QImage scene_image(_scene->getCanvasSize().first, _scene->getCanvasSize().second, QImage::Format_ARGB32);
    scene_image.fill(Qt::transparent);
    QPainter scene_painter(&scene_image);
    _scene->render(&scene_painter);
    scene_painter.end();
    
    int const actual_width = scene_image.width();
    int const actual_height = scene_image.height();

    int const fps = 30;// Set the desired frame rate
    _video_writer->open(filename, cv::VideoWriter::fourcc('X', '2', '6', '4'), fps, cv::Size(actual_width, actual_height), true);

    if (!_video_writer->isOpened()) {
        std::cout << "Could not open the output video file for write" << std::endl;
        return;
    }

    // Generate title sequence frames if enabled
    if (ui->title_sequence_groupbox->isChecked()) {
        int title_frame_count = ui->title_frames_spinbox->value();
        QString title_text = ui->title_text_edit->toPlainText();
        int font_size = ui->font_size_spinbox->value();
        
        for (int i = 0; i < title_frame_count; i++) {
            QImage title_frame = _generateTitleFrame(actual_width, actual_height, title_text, font_size);
            
            // Use the same conversion process as canvas frames
            _writeFrameToVideo(title_frame);
        }
    }

    connect(_scene, &Media_Window::canvasUpdated, this, &Export_Video_Widget::_handleCanvasUpdated);

    for (int i = start_num; i < end_num; i++) {
        _time_scrollbar->changeScrollBarValue(i);
    }

    disconnect(_scene, &Media_Window::canvasUpdated, this, &Export_Video_Widget::_handleCanvasUpdated);

    _video_writer->release();
}

void Export_Video_Widget::_handleCanvasUpdated(QImage const & canvasImage) {
    auto current_time = _data_manager->getCurrentTime();
    std::cout << "Saving frame " << current_time << std::endl;

    // Use the same conversion process as title frames
    _writeFrameToVideo(canvasImage);
}

void Export_Video_Widget::_updateTitlePreview() {
    if (!ui->title_sequence_groupbox->isChecked()) {
        ui->title_preview->setText("Title sequence disabled");
        ui->title_preview->setStyleSheet("background-color: gray; color: white; border: 1px solid gray;");
        return;
    }
    
    QString title_text = ui->title_text_edit->toPlainText();
    int font_size = ui->font_size_spinbox->value();
    
    // Generate a small preview image
    QImage preview_image = _generateTitleFrame(200, 100, title_text, font_size / 2); // Scale down font for preview
    
    // Convert to pixmap and set on label
    QPixmap preview_pixmap = QPixmap::fromImage(preview_image);
    ui->title_preview->setPixmap(preview_pixmap);
    ui->title_preview->setText(""); // Clear text when showing pixmap
    ui->title_preview->setStyleSheet("background-color: black; border: 1px solid gray;");
}

void Export_Video_Widget::_writeFrameToVideo(QImage const & frame) {
    // Ensure consistent format conversion for all frames
    QImage convertedImage = frame.convertToFormat(QImage::Format_RGB888);
    
    // Create cv::Mat with explicit stride to ensure consistency
    cv::Mat const mat(convertedImage.height(), convertedImage.width(), CV_8UC3, 
                     const_cast<uchar *>(convertedImage.bits()), convertedImage.bytesPerLine());
    
    // Convert RGB to BGR for OpenCV
    cv::Mat matBGR;
    cv::cvtColor(mat, matBGR, cv::COLOR_RGB2BGR);
    
    // Write the frame to the video
    _video_writer->write(matBGR);
}

QImage Export_Video_Widget::_generateTitleFrame(int width, int height, QString const & text, int font_size) {
    // Create title image with ARGB32 format to match typical canvas format
    QImage title_image(width, height, QImage::Format_ARGB32);
    title_image.fill(Qt::black);
    
    // Setup painter and font
    QPainter painter(&title_image);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QFont font("Arial", font_size, QFont::Bold);
    painter.setFont(font);
    painter.setPen(Qt::white);
    
    // Calculate text positioning for center alignment
    QFontMetrics font_metrics(font);
    QRect text_rect = font_metrics.boundingRect(QRect(0, 0, width, height), 
                                               Qt::AlignCenter | Qt::TextWordWrap, text);
    
    // Draw the text centered
    painter.drawText(QRect(0, 0, width, height), 
                    Qt::AlignCenter | Qt::TextWordWrap, text);
    
    painter.end();
    
    return title_image;
}
