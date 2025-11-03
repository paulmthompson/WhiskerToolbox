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

    explicit TimeScrollBar(QWidget *parent = nullptr);

    ~TimeScrollBar() override;

    void setDataManager(std::shared_ptr<DataManager> data_manager) {_data_manager = std::move(data_manager);};
    void updateScrollBarNewMax(int new_max);
    void changeScrollBarValue(int new_value, bool relative=false); // Should be friend

    int getFrameJumpValue() const;
    
    void PlayButton();

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
    
    /**
     * @brief Handle snap-to-keyframe logic for video data
     * @param current_frame The current frame position
     * @return The frame to snap to (may be the same as input if no snapping needed)
     */
    int _getSnapFrame(int current_frame);

private slots:
    void Slider_Drag(int newPos);
    void Slider_Scroll(int newPos);
    void RewindButton();
    void FastForwardButton();
    void FrameSpinBoxChanged(int frameNumber);
signals:
    void timeChanged(int x);
};


#endif // TIMESCROLLBAR_H
