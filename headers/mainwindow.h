#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QImage>
#include <QGraphicsScene>
#include <QTimer>

#include <memory>

#include "whiskertracker.h"
#include "Video_Window.h"

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
    void createActions();
    QImage convertToImage(std::vector<uint8_t> input, int width, int height);
    void DrawWhiskers();

    void vidLoop();

    int frame_count;

    QTimer* timer;

    int selected_whisker;

    long long t_last_draw;

    int play_speed;
    bool play_mode;

    Video_Window* scene;

    std::unique_ptr<WhiskerTracker> wt;

    enum Selection_Type {Whisker_Select,
                        Whisker_Pad_Select};
    MainWindow::Selection_Type selection_mode;

private slots:
    void Load_Video();
    void Slider_Drag(int newPos);
    void Slider_Scroll(int newPos);
    void PlayButton();
    void RewindButton();
    void FastForwardButton();
    void TraceButton();
    void ClickedInVideo(qreal,qreal);
    void addCovariate();
    void removeCovariate();
    void updateDisplay();
    void openWhiskerTracking();
};
#endif // MAINWINDOW_H
