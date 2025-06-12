#include "Export_Video_Widget.hpp"

#include "ui_Export_Video_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "TimeFrame.hpp"

#include "Media_Window/Media_Window.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"

#include "opencv2/opencv.hpp"
#include <QFileDialog>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QTableWidget>
#include <QTableWidgetItem>

#include <filesystem>
#include <iostream>
#include <regex>
#include <numbers>

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

    // Connect audio output controls
    connect(ui->audio_output_groupbox, &QGroupBox::toggled, this, &Export_Video_Widget::_updateDurationEstimate);
    connect(ui->audio_sources_table, &QTableWidget::itemChanged, this, &Export_Video_Widget::_onAudioSourceTableItemChanged);

    ui->start_frame_spinbox->setMaximum(_data_manager->getTime()->getTotalFrameCount());
    ui->end_frame_spinbox->setMaximum(_data_manager->getTime()->getTotalFrameCount());

    _video_writer = std::make_unique<cv::VideoWriter>();

    // Initialize output size to media dimensions
    _resetToMediaSize();

    // Initialize audio sources table
    _updateAudioSourcesTable();

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
            VideoSequence const & sequence = _video_sequences[seq_idx];

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

    // Generate audio track if enabled
    if (ui->audio_output_groupbox->isChecked()) {
        std::cout << "Generating audio track..." << std::endl;

        int start_frame, end_frame;
        if (!_video_sequences.empty()) {
            // For multi-sequence mode, generate audio for entire span
            start_frame = _video_sequences[0].start_frame;
            end_frame = _video_sequences.back().end_frame;
            for (auto const & seq: _video_sequences) {
                start_frame = std::min(start_frame, seq.start_frame);
                end_frame = std::max(end_frame, seq.end_frame);
            }
        } else {
            // Single sequence mode
            start_frame = ui->start_frame_spinbox->value();
            end_frame = ui->end_frame_spinbox->value();
            if (end_frame == -1) {
                end_frame = _data_manager->getTime()->getTotalFrameCount();
            }
        }

        int video_fps = ui->frame_rate_spinbox->value();
        int audio_sample_rate = ui->audio_sample_rate_spinbox->value();

        auto audio_track = _convertEventsToAudioTrack(start_frame, end_frame, video_fps, audio_sample_rate);

        // Generate audio filename based on video filename
        std::string audio_filename = filename;
        size_t last_dot = audio_filename.find_last_of('.');
        if (last_dot != std::string::npos) {
            audio_filename = audio_filename.substr(0, last_dot) + "_audio.wav";
        } else {
            audio_filename += "_audio.wav";
        }

        _writeAudioFile(audio_filename, audio_track, audio_sample_rate);
    }

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
    QImage preview_image = _generateTitleFrame(200, 100, title_text, font_size / 2);// Scale down font for preview

    // Convert to pixmap and set on label
    QPixmap preview_pixmap = QPixmap::fromImage(preview_image);
    ui->title_preview->setPixmap(preview_pixmap);
    ui->title_preview->setText("");// Clear text when showing pixmap
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
        VideoSequence const & seq = _video_sequences[i];

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
        for (VideoSequence const & seq: _video_sequences) {
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

void Export_Video_Widget::_updateAudioSourcesTable() {
    // Clear existing audio sources
    _audio_sources.clear();
    ui->audio_sources_table->setRowCount(0);

    if (!_data_manager) {
        return;
    }

    // Get all DigitalEventSeries keys from DataManager
    auto event_keys = _data_manager->getKeys<DigitalEventSeries>();

    // Populate audio sources vector and table
    for (auto const & key: event_keys) {
        auto series = _data_manager->getData<DigitalEventSeries>(key);
        if (!series) continue;

        std::string time_frame_key = _data_manager->getTimeFrame(key);
        int event_count = static_cast<int>(series->size());

        // Create audio source
        AudioSource audio_source(key, time_frame_key, event_count);
        _audio_sources.push_back(audio_source);

        // Add row to table
        int row = ui->audio_sources_table->rowCount();
        ui->audio_sources_table->insertRow(row);

        // Enabled checkbox
        auto * checkbox = new QTableWidgetItem();
        checkbox->setCheckState(Qt::Unchecked);
        checkbox->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        ui->audio_sources_table->setItem(row, 0, checkbox);

        // Event Series Key
        ui->audio_sources_table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(key)));

        // Time Frame
        ui->audio_sources_table->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(time_frame_key)));

        // Event Count
        ui->audio_sources_table->setItem(row, 3, new QTableWidgetItem(QString::number(event_count)));

        // Volume slider (as a spinbox for now)
        auto * volume_item = new QTableWidgetItem(QString::number(1.0, 'f', 1));
        ui->audio_sources_table->setItem(row, 4, volume_item);
    }

    // Resize columns to content
    ui->audio_sources_table->resizeColumnsToContents();

    std::cout << "Found " << event_keys.size() << " DigitalEventSeries for audio output" << std::endl;
}

void Export_Video_Widget::_onAudioSourceTableItemChanged(QTableWidgetItem * item) {
    if (!item) return;

    int row = item->row();
    int column = item->column();

    if (row >= 0 && row < static_cast<int>(_audio_sources.size())) {
        AudioSource & source = _audio_sources[row];

        if (column == 0) {// Enabled checkbox
            source.enabled = (item->checkState() == Qt::Checked);
            std::cout << "Audio source " << source.key << " "
                      << (source.enabled ? "enabled" : "disabled") << std::endl;
        } else if (column == 4) {// Volume
            bool ok;
            float volume = item->text().toFloat(&ok);
            if (ok && volume >= 0.0f && volume <= 2.0f) {
                source.volume = volume;
                std::cout << "Audio source " << source.key << " volume set to " << volume << std::endl;
            } else {
                // Reset to previous value if invalid
                item->setText(QString::number(source.volume, 'f', 1));
            }
        }

        // Update duration estimate when audio settings change
        _updateDurationEstimate();
    }
}

std::vector<float> Export_Video_Widget::_generateClickAudio(float duration_seconds, int sample_rate, double click_duration) const {
    int total_samples = static_cast<int>(duration_seconds * sample_rate);
    int click_samples = static_cast<int>(click_duration * sample_rate);

    std::vector<float> audio_data(total_samples, 0.0f);

    // Generate a simple click sound (sine wave burst)
    float frequency = 1000.0f;// 1kHz click
    for (int i = 0; i < std::min(click_samples, total_samples); ++i) {
        float t = static_cast<float>(i) / static_cast<float>(sample_rate);
        float amplitude = std::exp(-t * 20.0f);// Exponential decay
        audio_data[i] = amplitude * std::sin(2.0f * std::numbers::pi * frequency * t);
    }

    return audio_data;
}

std::vector<float> Export_Video_Widget::_convertEventsToAudioTrack(int start_frame, int end_frame, int video_fps, int audio_sample_rate) const {
    // Calculate total duration in seconds
    float duration_seconds = static_cast<float>(end_frame - start_frame) / static_cast<float>(video_fps);
    int total_samples = static_cast<int>(duration_seconds * audio_sample_rate);

    std::vector<float> audio_track(total_samples, 0.0f);

    if (!ui->audio_output_groupbox->isChecked()) {
        return audio_track;// Return silent track if audio is disabled
    }

    double click_duration = ui->click_duration_spinbox->value();

    // Get master time frame for conversion (similar to OpenGLWidget approach)
    auto master_time_frame = _data_manager->getTime("time");
    if (!master_time_frame) {
        std::cerr << "Error: Could not get master time frame for audio conversion" << std::endl;
        return audio_track;
    }

    // Process each enabled audio source
    for (auto const & audio_source: _audio_sources) {
        if (!audio_source.enabled) continue;

        auto series = _data_manager->getData<DigitalEventSeries>(audio_source.key);
        if (!series) continue;

        auto series_time_frame = _data_manager->getTime(audio_source.time_frame_key);
        if (!series_time_frame) continue;

        // Get events in the frame range, handling timeframe conversion similar to OpenGLWidget
        auto start_time = static_cast<float>(start_frame);
        auto end_time = static_cast<float>(end_frame);

        // Convert master time coordinates to series time frame if needed
        std::vector<float> converted_events;
        if (series_time_frame.get() == master_time_frame.get()) {
            // Same time frame - use events directly
            auto events_in_range = series->getEventsAsVector(start_time, end_time);
            converted_events = events_in_range;
        } else {
            // Different time frames - convert each event from series to master coordinates
            auto all_events = series->getEventSeries();
            for (float event_idx: all_events) {
                try {
                    // Convert series time index to actual time, then to master time frame
                    float event_time = static_cast<float>(series_time_frame->getTimeAtIndex(TimeIndex(static_cast<int>(event_idx))));

                    // Check if this event time falls within our frame range
                    if (event_time >= start_time && event_time <= end_time) {
                        converted_events.push_back(event_time);
                    }
                } catch (...) {
                    // Skip events that can't be converted
                    continue;
                }
            }
        }

        // Generate click sounds for each event
        for (float event_time: converted_events) {
            // Convert event time to audio sample index
            float relative_time = (event_time - start_time) / static_cast<float>(video_fps);
            int sample_index = static_cast<int>(relative_time * audio_sample_rate);

            if (sample_index >= 0 && sample_index < total_samples) {
                // Generate click audio
                auto click_audio = _generateClickAudio(click_duration, audio_sample_rate, click_duration);

                // Mix click into audio track with volume scaling
                for (int i = 0; i < static_cast<int>(click_audio.size()) && (sample_index + i) < total_samples; ++i) {
                    audio_track[sample_index + i] += click_audio[i] * audio_source.volume;
                }
            }
        }

        std::cout << "Added " << converted_events.size() << " audio clicks from series " << audio_source.key << std::endl;
    }

    return audio_track;
}

void Export_Video_Widget::_writeAudioFile(std::string const & audio_filename, std::vector<float> const & audio_data, int sample_rate) const {
    // For now, we'll output a simple informational message about audio generation
    // In a full implementation, you would use a library like libsndfile, Qt's audio classes,
    // or integrate with FFmpeg to create an actual audio file

    std::cout << "Audio track generated: " << audio_data.size() << " samples at " << sample_rate << " Hz" << std::endl;
    std::cout << "Duration: " << static_cast<float>(audio_data.size()) / static_cast<float>(sample_rate) << " seconds" << std::endl;
    std::cout << "Note: Audio file writing not yet implemented. Consider using FFmpeg for video+audio combination." << std::endl;

    // TODO: Implement actual audio file writing
    // This could involve:
    // 1. Writing a WAV file directly
    // 2. Using Qt's audio classes
    // 3. Using FFmpeg to combine video and audio
    // 4. Using a dedicated audio library like libsndfile
}
