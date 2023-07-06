#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QImage>
#include <QGraphicsScene>
#include <QTimer>
#include <QPointer>

#include <memory>

#include "Video_Window.h"
#include "Whisker_Widget.h"

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

private:
    Ui::MainWindow *ui;

    Video_Window* scene;
    QPointer<Whisker_Widget> ww;

    void createActions();
    QImage convertToImage(std::vector<uint8_t> input, int width, int height);

    void vidLoop();

    int frame_count;

    QTimer* timer;

    long long t_last_draw;

    int play_speed;
    bool play_mode;

private slots:
    void Load_Video();
    void Slider_Drag(int newPos);
    void Slider_Scroll(int newPos);
    void PlayButton();
    void RewindButton();
    void FastForwardButton();

    void addCovariate();
    void removeCovariate();
    void updateDisplay();
    void openWhiskerTracking();
};
#endif // MAINWINDOW_H
