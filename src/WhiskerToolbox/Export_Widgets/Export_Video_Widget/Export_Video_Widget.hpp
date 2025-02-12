#ifndef EXPORT_VIDEO_WIDGET_HPP
#define EXPORT_VIDEO_WIDGET_HPP

#include <QImage>
#include <QWidget>

#include <memory>
#include <string>

class DataManager;
class Media_Window;
class TimeScrollBar;

namespace cv {
    class VideoWriter;
}

namespace Ui {
class Export_Video_Widget;
}

class Export_Video_Widget : public QWidget
{
    Q_OBJECT
public:

    Export_Video_Widget(
            std::shared_ptr<DataManager> data_manager,
            Media_Window* scene,
            TimeScrollBar* time_scrollbar,
            QWidget *parent = 0);
    ~Export_Video_Widget();

    void openWidget();

private:
    Ui::Export_Video_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window* _scene;
    TimeScrollBar* _time_scrollbar;
    std::unique_ptr<cv::VideoWriter> _video_writer;

private slots:
    void _exportVideo();
    void _handleCanvasUpdated(const QImage canvasImage);
};



#endif // EXPORT_VIDEO_WIDGET_HPP
