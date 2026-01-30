#ifndef EXPORT_VIDEO_WIDGET_HPP
#define EXPORT_VIDEO_WIDGET_HPP

#include "EditorState/StrongTypes.hpp" // Qt metatype registration for TimePosition
#include "TimeFrame/TimeFrame.hpp"  // For TimePosition

#include <QImage>
#include <QWidget>

#include <memory>
#include <string>
#include <vector>



class DataManager;
class EditorRegistry;
class ExportVideoWidgetState;
class MediaWidgetState;
class DigitalEventSeries;
class TimeFrame;
class QTableWidgetItem;

namespace cv {
class VideoWriter;
}

namespace Ui {
class Export_Video_Widget;
}

struct VideoSequence {
    int start_frame{0};
    int end_frame{0};
    bool has_title{false};
    QString title_text;
    int title_frames{100};
    int title_font_size{24};

    VideoSequence() = default;
    VideoSequence(int start, int end, bool title = false, QString const & text = "", int frames = 100, int font = 24)
        : start_frame(start),
          end_frame(end),
          has_title(title),
          title_text(text),
          title_frames(frames),
          title_font_size(font) {}
};

struct AudioSource {
    std::string key;
    std::string time_frame_key;
    bool enabled{false};
    float volume{1.0f};
    int event_count{0};

    AudioSource() = default;
    AudioSource(std::string const & k, std::string const & tf, int count = 0)
        : key(k),
          time_frame_key(tf),
          event_count(count) {}
};

class Export_Video_Widget : public QWidget {
    Q_OBJECT
public:
    Export_Video_Widget(
            std::shared_ptr<DataManager> data_manager,
            EditorRegistry * editor_registry,
            std::shared_ptr<ExportVideoWidgetState> state,
            QWidget * parent = nullptr);
    ~Export_Video_Widget() override;

    void openWidget();

    /**
     * @brief Get the widget's state object
     * @return Shared pointer to ExportVideoWidgetState
     */
    [[nodiscard]] std::shared_ptr<ExportVideoWidgetState> state() const { return _state; }

signals:
    /**
     * @brief Emitted when widget requests a time change (e.g., during video export)
     * 
     * This signal is connected to EditorRegistry::setCurrentTime() at registration time.
     * The widget emits this signal to request time changes during video export,
     * allowing other widgets to update synchronously.
     * 
     * @param position The TimePosition to navigate to
     */
    void requestTimeChange(TimePosition position);

private:
    Ui::Export_Video_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    EditorRegistry * _editor_registry;
    std::shared_ptr<ExportVideoWidgetState> _state;
    std::unique_ptr<cv::VideoWriter> _video_writer;

    // Currently selected media widget state for export
    std::shared_ptr<MediaWidgetState> _selected_state;

    // Multi-sequence support
    std::vector<VideoSequence> _video_sequences;

    // Audio support
    std::vector<AudioSource> _audio_sources;

    // Frame tracking to prevent duplicate writes
    int64_t _last_written_frame{-1};

private slots:
    void _exportVideo();
    void _handleCanvasUpdated(QImage const & canvasImage);
    void _updateTitlePreview();
    void _updateDurationEstimate();
    void _addSequence();
    void _removeSequence();
    void _updateSequenceTable();
    void _resetToMediaSize();
    void _updateAudioSourcesTable();
    void _onAudioSourceTableItemChanged(QTableWidgetItem * item);
    void _onMediaWidgetSelectionChanged();

private:
    QImage _generateTitleFrame(int width, int height, QString const & text, int font_size);
    void _writeFrameToVideo(QImage const & frame);
    std::pair<int, int> _getMediaDimensions() const;
    std::shared_ptr<MediaWidgetState> _getSelectedState() const;
    void _updateMediaWidgetComboBox();

    // Audio generation methods
    std::vector<float> _generateClickAudio(float duration_seconds, int sample_rate, double click_duration) const;
    std::vector<float> _convertEventsToAudioTrack(int start_frame, int end_frame, int video_fps, int audio_sample_rate) const;
    void _writeAudioFile(std::string const & audio_filename, std::vector<float> const & audio_data, int sample_rate) const;

};


#endif// EXPORT_VIDEO_WIDGET_HPP
