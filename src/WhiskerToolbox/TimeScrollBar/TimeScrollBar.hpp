#ifndef TIMESCROLLBAR_H
#define TIMESCROLLBAR_H

#include <QWidget>

#include <memory>

class DataManager;
class QTimer;
class TimeScrollBarState;

namespace Ui {
class TimeScrollBar;
}

class TimeScrollBar : public QWidget
{
    Q_OBJECT
public:

    /**
     * @brief Construct TimeScrollBar with EditorState support
     * 
     * This is the preferred constructor when using EditorRegistry.
     * The state manages serializable configuration and enables
     * workspace save/restore.
     * 
     * @param data_manager Shared DataManager for data access
     * @param state Shared TimeScrollBarState for configuration persistence
     * @param parent Parent widget
     */
    explicit TimeScrollBar(std::shared_ptr<DataManager> data_manager,
                           std::shared_ptr<TimeScrollBarState> state,
                           QWidget * parent = nullptr);

    /**
     * @brief Legacy constructor without state (backward compatible)
     * @deprecated Use the constructor with TimeScrollBarState instead
     */
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
    std::shared_ptr<TimeScrollBarState> _state;  // EditorState for serialization
    
    bool _verbose {false};
    int _play_speed {1};
    bool _play_mode {false};

    QTimer* _timer;


    void _updateFrameLabels(int frame_num);
    void _vidLoop();
    
    // State management helpers
    void _setupConnections();
    void _initializeFromState();
    
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
