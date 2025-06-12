
#include "Export_Video_Widget.hpp"

#include "ui_Export_Video_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"

#include "Media_Window/Media_Window.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"

#include "opencv2/opencv.hpp"
#include <QFileDialog>
#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QTableWidget>
#include <QTableWidgetItem>

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
    
    // Connect sequence management buttons
    connect(ui->add_sequence_button, &QPushButton::clicked, this, &Export_Video_Widget::_addSequence);
    connect(ui->remove_sequence_button, &QPushButton::clicked, this, &Export_Video_Widget::_removeSequence);
    connect(ui->sequences_table, &QTableWidget::itemSelectionChanged, this, [this]() {
        ui->remove_sequence_button->setEnabled(ui->sequences_table->currentRow() >= 0);
    });
    
    // Connect output size controls
    connect(ui->reset_to_media_size_button, &QPushButton::clicked, this, &Export_Video_Widget::_resetToMediaSize);

    ui->start_frame_spinbox->setMaximum(_data_manager->getTime()->getTotalFrameCount());
    ui->end_frame_spinbox->setMaximum(_data_manager->getTime()->getTotalFrameCount());

    _video_writer = std::make_unique<cv::VideoWriter>();
    
    // Initialize output size to media dimensions
    _resetToMediaSize();
    
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
    auto filename = ui->output_filename->text().toStdString();

    // If filename doesn't have .mp4 as ending, add it
    if (!std::regex_match(filename, std::regex(".*\\.mp4"))) {
        filename += ".mp4";
    }

    // Get configured output dimensions
    int output_width = ui->output_width_spinbox->value();
    int output_height = ui->output_height_spinbox->value();
    
    std::cout << "Exporting video with output dimensions: " << output_width << "x" << output_height << std::endl;

    int const fps = ui->frame_rate_spinbox->value();// Get frame rate from user control
    std::cout << "Using frame rate: " << fps << " fps" << std::endl;
    _video_writer->open(filename, cv::VideoWriter::fourcc('X', '2', '6', '4'), fps, cv::Size(output_width, output_height), true);

    if (!_video_writer->isOpened()) {
        std::cout << "Could not open the output video file for write" << std::endl;
        return;
    }

    connect(_scene, &Media_Window::canvasUpdated, this, &Export_Video_Widget::_handleCanvasUpdated);

    if (!_video_sequences.empty()) {
        // Multi-sequence mode
        std::cout << "Exporting " << _video_sequences.size() << " sequences" << std::endl;
        
        for (size_t seq_idx = 0; seq_idx < _video_sequences.size(); ++seq_idx) {
            const VideoSequence& sequence = _video_sequences[seq_idx];
            
            std::cout << "Processing sequence " << (seq_idx + 1) << ": frames " 
                      << sequence.start_frame << "-" << sequence.end_frame << std::endl;
            
            // Generate title sequence for this sequence if enabled
            if (sequence.has_title) {
                std::cout << "Generating " << sequence.title_frames << " title frames for sequence " << (seq_idx + 1) << std::endl;
                
                for (int i = 0; i < sequence.title_frames; i++) {
                    QImage title_frame = _generateTitleFrame(output_width, output_height, 
                                                           sequence.title_text, sequence.title_font_size);
                    _writeFrameToVideo(title_frame);
                }
            }
            
            // Export content frames for this sequence
            for (int i = sequence.start_frame; i < sequence.end_frame; i++) {
                _time_scrollbar->changeScrollBarValue(i);
            }
        }
    } else {
        // Single sequence mode (existing behavior)
        auto start_num = ui->start_frame_spinbox->value();
        auto end_num = ui->end_frame_spinbox->value();

        if (end_num == -1) {
            end_num = _data_manager->getTime()->getTotalFrameCount();
        }

        if (start_num >= end_num) {
            std::cout << "Start frame must be less than end frame" << std::endl;
            disconnect(_scene, &Media_Window::canvasUpdated, this, &Export_Video_Widget::_handleCanvasUpdated);
            _video_writer->release();
            return;
        }

        std::cout << "Exporting single sequence: frames " << start_num << "-" << end_num << std::endl;

        // Generate title sequence frames if enabled
        if (ui->title_sequence_groupbox->isChecked()) {
            int title_frame_count = ui->title_frames_spinbox->value();
            QString title_text = ui->title_text_edit->toPlainText();
            int font_size = ui->font_size_spinbox->value();
            
            std::cout << "Generating " << title_frame_count << " title frames" << std::endl;
            
            for (int i = 0; i < title_frame_count; i++) {
                QImage title_frame = _generateTitleFrame(output_width, output_height, title_text, font_size);
                _writeFrameToVideo(title_frame);
            }
        }

        // Export content frames
        for (int i = start_num; i < end_num; i++) {
            _time_scrollbar->changeScrollBarValue(i);
        }
    }

    disconnect(_scene, &Media_Window::canvasUpdated, this, &Export_Video_Widget::_handleCanvasUpdated);
    _video_writer->release();
    
    std::cout << "Video export completed: " << filename << std::endl;
}

void Export_Video_Widget::_handleCanvasUpdated(QImage const & canvasImage) {
    auto current_time = _data_manager->getCurrentTime();
    std::cout << "Saving frame " << current_time << std::endl;

    // Resize canvas image to output dimensions
    int output_width = ui->output_width_spinbox->value();
    int output_height = ui->output_height_spinbox->value();
    
    QImage resized_image = canvasImage.scaled(output_width, output_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    
    // Use the same conversion process as title frames
    _writeFrameToVideo(resized_image);
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

void Export_Video_Widget::_addSequence() {
    int start_frame = ui->start_frame_spinbox->value();
    int end_frame = ui->end_frame_spinbox->value();
    
    // Handle end_frame of -1 (use total frame count)
    if (end_frame == -1) {
        end_frame = _data_manager->getTime()->getTotalFrameCount();
    }
    
    // Validate frame range
    if (start_frame >= end_frame) {
        std::cout << "Invalid frame range: start frame must be less than end frame" << std::endl;
        return;
    }
    
    // Create new sequence with current title settings
    bool has_title = ui->title_sequence_groupbox->isChecked();
    QString title_text = has_title ? ui->title_text_edit->toPlainText() : "";
    int title_frames = has_title ? ui->title_frames_spinbox->value() : 100;
    int title_font_size = has_title ? ui->font_size_spinbox->value() : 24;
    
    VideoSequence sequence(start_frame, end_frame, has_title, title_text, title_frames, title_font_size);
    _video_sequences.push_back(sequence);
    
    _updateSequenceTable();
    _updateDurationEstimate();
    
    std::cout << "Added sequence: frames " << start_frame << "-" << end_frame 
              << (has_title ? " with title" : "") << std::endl;
}

void Export_Video_Widget::_removeSequence() {
    int selected_row = ui->sequences_table->currentRow();
    if (selected_row >= 0 && selected_row < static_cast<int>(_video_sequences.size())) {
        _video_sequences.erase(_video_sequences.begin() + selected_row);
        _updateSequenceTable();
        _updateDurationEstimate();
        
        std::cout << "Removed sequence at row " << selected_row << std::endl;
    }
}

void Export_Video_Widget::_updateSequenceTable() {
    ui->sequences_table->setRowCount(static_cast<int>(_video_sequences.size()));
    
    for (int i = 0; i < static_cast<int>(_video_sequences.size()); ++i) {
        const VideoSequence& seq = _video_sequences[i];
        
        // Start Frame
        ui->sequences_table->setItem(i, 0, new QTableWidgetItem(QString::number(seq.start_frame)));
        
        // End Frame
        ui->sequences_table->setItem(i, 1, new QTableWidgetItem(QString::number(seq.end_frame)));
        
        // Has Title
        ui->sequences_table->setItem(i, 2, new QTableWidgetItem(seq.has_title ? "Yes" : "No"));
        
        // Title Text (truncated for display)
        QString display_text = seq.title_text;
        if (display_text.length() > 30) {
            display_text = display_text.left(27) + "...";
        }
        ui->sequences_table->setItem(i, 3, new QTableWidgetItem(display_text));
        
        // Title Frames
        ui->sequences_table->setItem(i, 4, new QTableWidgetItem(QString::number(seq.title_frames)));
    }
    
    // Resize columns to content
    ui->sequences_table->resizeColumnsToContents();
}

void Export_Video_Widget::_updateDurationEstimate() {
    int frame_rate = ui->frame_rate_spinbox->value();
    int total_frames = 0;
    
    if (!_video_sequences.empty()) {
        // Multi-sequence mode
        for (const VideoSequence& seq : _video_sequences) {
            // Add content frames
            if (seq.start_frame < seq.end_frame) {
                total_frames += (seq.end_frame - seq.start_frame);
                
                // Add title frames if enabled for this sequence
                if (seq.has_title) {
                    total_frames += seq.title_frames;
                }
            }
        }
        
        if (total_frames > 0 && frame_rate > 0) {
            double duration_seconds = static_cast<double>(total_frames) / static_cast<double>(frame_rate);
            
            QString duration_text;
            if (duration_seconds >= 60.0) {
                int minutes = static_cast<int>(duration_seconds / 60.0);
                double remaining_seconds = duration_seconds - (minutes * 60.0);
                duration_text = QString("Estimated Duration: %1m %2s (%3 frames, %4 sequences)")
                               .arg(minutes)
                               .arg(remaining_seconds, 0, 'f', 1)
                               .arg(total_frames)
                               .arg(_video_sequences.size());
            } else {
                duration_text = QString("Estimated Duration: %1 seconds (%2 frames, %3 sequences)")
                               .arg(duration_seconds, 0, 'f', 1)
                               .arg(total_frames)
                               .arg(_video_sequences.size());
            }
            
            ui->duration_estimate_label->setText(duration_text);
            ui->duration_estimate_label->setStyleSheet("color: blue; font-weight: bold;");
        } else {
            ui->duration_estimate_label->setText("Estimated Duration: Invalid sequences");
            ui->duration_estimate_label->setStyleSheet("color: red; font-weight: bold;");
        }
    } else {
        // Single sequence mode (existing behavior)
        int start_frame = ui->start_frame_spinbox->value();
        int end_frame = ui->end_frame_spinbox->value();
        
        // Handle end_frame of -1 (use total frame count)
        if (end_frame == -1) {
            end_frame = _data_manager->getTime()->getTotalFrameCount();
        }
        
        // Calculate video duration
        if (start_frame < end_frame && frame_rate > 0) {
            total_frames = end_frame - start_frame;
            
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
}

std::pair<int, int> Export_Video_Widget::_getMediaDimensions() const {
    // Try to get media dimensions from DataManager
    auto media_data = _data_manager->getData<MediaData>("media");
    if (media_data != nullptr) {
            int width = media_data->getWidth();
            int height = media_data->getHeight();
            std::cout << "Retrieved media dimensions: " << width << "x" << height << std::endl;
        return std::make_pair(width, height);
    }
    
    // Fallback to common video resolution
    std::cout << "Using fallback dimensions: 1920x1080" << std::endl;
    return std::make_pair(1920, 1080);
}

void Export_Video_Widget::_resetToMediaSize() {
    auto [width, height] = _getMediaDimensions();
    
    ui->output_width_spinbox->setValue(width);
    ui->output_height_spinbox->setValue(height);
    
    std::cout << "Reset output size to media dimensions: " << width << "x" << height << std::endl;
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
