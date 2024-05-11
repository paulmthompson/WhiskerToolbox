#ifndef TIME_SCROLLBAR_H
#define TIME_SCROLLBAR_H

#include "DataManager.hpp"

#include <QWidget>
#include <QTimer>

#include <memory>
#include <vector>

namespace Ui {
class TimeScrollBar;
}

class TimeScrollBar : public QWidget
{
    Q_OBJECT
public:

    TimeScrollBar(std::shared_ptr<DataManager> data_manager, QWidget *parent = 0);

    virtual ~TimeScrollBar();

protected:
private:
    Ui::TimeScrollBar *ui;
    std::shared_ptr<DataManager> _data_manager;
    bool _verbose;
    int _play_speed;
    bool _play_mode;

    QTimer* _timer;

    void _updateScrollBarNewMax(int new_max);
    void _updateFrameLabels(int frame_num);
    void _vidLoop();

private slots:
    void Slider_Drag(int newPos);
    void Slider_Scroll(int newPos);
    void PlayButton();
    void RewindButton();
    void FastForwardButton();
};


#endif // TIME_SCROLLBAR_H
