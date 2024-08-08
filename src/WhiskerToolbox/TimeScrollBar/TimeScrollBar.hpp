#ifndef TIMESCROLLBAR_H
#define TIMESCROLLBAR_H

#include <QWidget>

#include <memory>

class DataManager;
class QTimer;

namespace Ui {
class TimeScrollBar;
}

class TimeScrollBar : public QWidget
{
    Q_OBJECT
public:

    explicit TimeScrollBar(QWidget *parent = 0);

    virtual ~TimeScrollBar();

    void setDataManager(std::shared_ptr<DataManager> data_manager) {_data_manager = data_manager;};
    void updateScrollBarNewMax(int new_max);
    void changeScrollBarValue(int new_value, bool relative=false); // Should be friend

protected:
private:
    Ui::TimeScrollBar *ui;
    std::shared_ptr<DataManager> _data_manager;
    bool _verbose {false};
    int _play_speed {1};
    bool _play_mode {false};

    QTimer* _timer;


    void _updateFrameLabels(int frame_num);
    void _vidLoop();

private slots:
    void Slider_Drag(int newPos);
    void Slider_Scroll(int newPos);
    void PlayButton();
    void RewindButton();
    void FastForwardButton();
signals:
    void timeChanged(int x);
};


#endif // TIMESCROLLBAR_H
