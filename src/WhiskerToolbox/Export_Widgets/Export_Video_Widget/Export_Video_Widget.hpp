#ifndef EXPORT_VIDEO_WIDGET_HPP
#define EXPORT_VIDEO_WIDGET_HPP

#include <QImage>
#include <QWidget>

#include <memory>
#include <string>
#include <vector>

class DataManager;
class Media_Window;
class TimeScrollBar;

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
    VideoSequence(int start, int end, bool title = false, const QString& text = "", int frames = 100, int font = 24)
        : start_frame(start), end_frame(end), has_title(title), title_text(text), title_frames(frames), title_font_size(font) {}
};

class Export_Video_Widget : public QWidget {
    Q_OBJECT
public:
    Export_Video_Widget(
            std::shared_ptr<DataManager> data_manager,
            Media_Window * scene,
            TimeScrollBar * time_scrollbar,
            QWidget * parent = nullptr);
    ~Export_Video_Widget() override;

    void openWidget();

private:
    Ui::Export_Video_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window * _scene;
    TimeScrollBar * _time_scrollbar;
    std::unique_ptr<cv::VideoWriter> _video_writer;
    
    // Multi-sequence support
    std::vector<VideoSequence> _video_sequences;

private slots:
    void _exportVideo();
    void _handleCanvasUpdated(QImage const & canvasImage);
    void _updateTitlePreview();
    void _updateDurationEstimate();
    void _addSequence();
    void _removeSequence();
    void _updateSequenceTable();

private:
    QImage _generateTitleFrame(int width, int height, QString const & text, int font_size);
    void _writeFrameToVideo(QImage const & frame);
};


#endif// EXPORT_VIDEO_WIDGET_HPP
