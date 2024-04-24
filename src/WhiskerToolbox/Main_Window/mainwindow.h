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

#include "TimeFrame.hpp"
#include "DataManager.hpp"

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

    Media_Window* _scene;
    QPointer<Whisker_Widget> _ww;
    QPointer<Label_Widget> _label_maker;
    QPointer<Analog_Viewer> _analog_viewer;

    void _updateMedia();

    void _createActions();

    void _vidLoop();

    void _LoadData(std::string filepath);

    void _updateScrollBarNewMax(int new_max);
    void _updateDataDisplays(int advance_n_frames);
    void _updateFrameLabels(int frame_num);

    QTimer* _timer;

    long long _t_last_draw;

    int _play_speed;
    bool _play_mode;

    bool _verbose;

    void _LoadFrame(int frame_id);

    std::shared_ptr<DataManager> _data_manager;


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
