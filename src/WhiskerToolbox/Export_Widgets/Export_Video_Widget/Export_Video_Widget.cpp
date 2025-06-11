
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
    
    // Connect title sequence settings for duration estimate updates
    connect(ui->title_sequence_groupbox, &QGroupBox::toggled, this, &Export_Video_Widget::_updateDurationEstimate);
    connect(ui->title_frames_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), this, &Export_Video_Widget::_updateDurationEstimate);
    
    // Connect frame controls for duration estimate updates
    connect(ui->start_frame_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), this, &Export_Video_Widget::_updateDurationEstimate);
    connect(ui->end_frame_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), this, &Export_Video_Widget::_updateDurationEstimate);
    connect(ui->frame_rate_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), this, &Export_Video_Widget::_updateDurationEstimate);

    ui->start_frame_spinbox->setMaximum(_data_manager->getTime()->getTotalFrameCount());
    ui->end_frame_spinbox->setMaximum(_data_manager->getTime()->getTotalFrameCount());

    _video_writer = std::make_unique<cv::VideoWriter>();
    
    // Initialize title preview and duration estimate
    _updateTitlePreview();
    _updateDurationEstimate();
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

    // Get canvas dimensions at export time and use consistently throughout
    auto [canvas_width, canvas_height] = _scene->getCanvasSize();
    
    std::cout << "Exporting video with canvas dimensions: " << canvas_width << "x" << canvas_height << std::endl;

    int const fps = ui->frame_rate_spinbox->value();// Get frame rate from user control
    std::cout << "Using frame rate: " << fps << " fps" << std::endl;
    _video_writer->open(filename, cv::VideoWriter::fourcc('X', '2', '6', '4'), fps, cv::Size(canvas_width, canvas_height), true);

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
            QImage title_frame = _generateTitleFrame(canvas_width, canvas_height, title_text, font_size);
            
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

void Export_Video_Widget::_updateDurationEstimate() {
    int start_frame = ui->start_frame_spinbox->value();
    int end_frame = ui->end_frame_spinbox->value();
    int frame_rate = ui->frame_rate_spinbox->value();
    
    // Handle end_frame of -1 (use total frame count)
    if (end_frame == -1) {
        end_frame = _data_manager->getTime()->getTotalFrameCount();
    }
    
    // Calculate video duration
    if (start_frame < end_frame && frame_rate > 0) {
        int total_frames = end_frame - start_frame;
        
        // Add title sequence frames if enabled
        if (ui->title_sequence_groupbox->isChecked()) {
            total_frames += ui->title_frames_spinbox->value();
        }
        
        double duration_seconds = static_cast<double>(total_frames) / static_cast<double>(frame_rate);
        
        // Format duration nicely (show minutes if > 60 seconds)
        QString duration_text;
        if (duration_seconds >= 60.0) {
            int minutes = static_cast<int>(duration_seconds / 60.0);
            double remaining_seconds = duration_seconds - (minutes * 60.0);
            duration_text = QString("Estimated Duration: %1m %2s (%3 frames)")
                           .arg(minutes)
                           .arg(remaining_seconds, 0, 'f', 1)
                           .arg(total_frames);
        } else {
            duration_text = QString("Estimated Duration: %1 seconds (%2 frames)")
                           .arg(duration_seconds, 0, 'f', 1)
                           .arg(total_frames);
        }
        
        ui->duration_estimate_label->setText(duration_text);
        ui->duration_estimate_label->setStyleSheet("color: blue; font-weight: bold;");
    } else {
        ui->duration_estimate_label->setText("Estimated Duration: Invalid frame range");
        ui->duration_estimate_label->setStyleSheet("color: red; font-weight: bold;");
    }
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
