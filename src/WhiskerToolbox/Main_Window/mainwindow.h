#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QImage>
#include <QGraphicsScene>
#include <QTimer>
#include <QPointer>

#include "Media_Window.h"
#include "Whisker_Widget.h"
#include "Label_Widget.h"
#include "analog_viewer.hpp"

#include "TimeFrame.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event);

private:
    Ui::MainWindow *ui;

    Media_Window* scene;
    QPointer<Whisker_Widget> ww;
    QPointer<Label_Widget> label_maker;
    QPointer<Analog_Viewer> _analog_viewer;

    void updateMedia();

    void createActions();

    void vidLoop();

    void LoadData(std::string filepath);

    void updateScrollBarNewMax(int new_max);
    void updateDataDisplays(int advance_n_frames);
    void updateFrameLabels(int frame_num);

    QTimer* timer;

    long long t_last_draw;

    int play_speed;
    bool play_mode;

    bool verbose;

    void LoadFrame(int frame_id);

    std::shared_ptr<TimeFrame> time;


private slots:
    void Load_Video();
    void Load_Images();
    void Slider_Drag(int newPos);
    void Slider_Scroll(int newPos);
    void PlayButton();
    void RewindButton();
    void FastForwardButton();

    void addCovariate();
    void removeCovariate();
    void updateDisplay();
    void openWhiskerTracking();
    void openLabelMaker();
    void openAnalogViewer();
};
#endif // MAINWINDOW_H
