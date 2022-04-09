#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QImage>
#include <QGraphicsScene>
#include <QTimer>

#include <memory>

#include "videodecoder.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    void createActions();
    QImage convertToImage(std::vector<uint8_t> input, int width, int height);
    void UpdateCanvas(QImage& img);
    void GetVideoInfo();
    void LoadFrame(int frame_id,bool frame_by_frame = false);
    void DrawWhiskers();

    void vidLoop();

    std::unique_ptr<VideoDecoder> vd;

    QString vid_name;
    int frame_count;

    QTimer* timer;



    long long t_last_draw;
    int last_loaded_frame;

    int play_speed;
    bool play_mode;

    QImage myimage;
    QGraphicsScene* scene;
    QGraphicsPixmapItem* pixmap_item;
    QVector<QGraphicsPathItem*> whisker_paths;

private slots:
    void Load_Video();
    void Slider_Scroll(int newPos);
    void PlayButton();
    void RewindButton();
    void FastForwardButton();
    void TraceButton();
};
#endif // MAINWINDOW_H
