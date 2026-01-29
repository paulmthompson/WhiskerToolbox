#ifndef TIMESCROLLBAR_H
#define TIMESCROLLBAR_H

#include <QWidget>

#include <memory>

#include "TimeFrame/TimeFrame.hpp"  // For TimePosition, TimeFrameIndex
#include "TimeFrame/StrongTimeTypes.hpp"  // For TimeKey

class DataManager;
class EditorRegistry;
class QComboBox;
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

    /**
     * @brief Set the DataManager and register for notifications
     * 
     * When a DataManager is set, TimeScrollBar will listen for notifications
     * and automatically update its timeframe when data changes.
     * 
     * @param data_manager Shared DataManager instance
     */
    void setDataManager(std::shared_ptr<DataManager> data_manager);
    void updateScrollBarNewMax(int new_max);
    void changeScrollBarValue(int new_value, bool relative=false); // Should be friend

    int getFrameJumpValue() const;
    
    void PlayButton();

    /**
     * @brief Set the EditorRegistry for time synchronization
     * 
     * TimeScrollBar will call EditorRegistry::setCurrentTime() when the user
     * scrubs the timeline, ensuring all widgets stay synchronized.
     * 
     * @param registry The EditorRegistry instance (can be nullptr)
     */
    void setEditorRegistry(EditorRegistry * registry);

    /**
     * @brief Set which TimeFrame this scrollbar controls
     * 
     * Updates the scrollbar to control the specified TimeFrame. The display_key
     * is used for UI labels (e.g., "Camera Time", "Ephys Clock").
     * 
     * @param tf The TimeFrame to control (can be nullptr)
     * @param display_key The TimeKey for UI display (defaults to "time")
     */
    void setTimeFrame(std::shared_ptr<TimeFrame> tf, TimeKey display_key = TimeKey("time"));

protected:
private:
    Ui::TimeScrollBar *ui;
    std::shared_ptr<DataManager> _data_manager;
    std::shared_ptr<TimeScrollBarState> _state;  // EditorState for serialization
    EditorRegistry * _editor_registry{nullptr};  // For time synchronization
    
    // TimeFrame management
    std::shared_ptr<TimeFrame> _current_time_frame;  // The TimeFrame this scrollbar controls
    TimeKey _current_display_key{TimeKey("time")};   // For UI display only
    
    // DataManager observer
    int _data_manager_observer_id{-1};  // Observer ID for DataManager notifications
    
    bool _verbose {true};
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

    /**
     * @brief Populate the TimeKey selector ComboBox with available TimeKeys
     */
    void _populateTimeKeySelector();

    /**
     * @brief Handle TimeKey selection change from UI
     * @param key_str The selected TimeKey as a string
     */
    void _onTimeKeyChanged(QString const & key_str);

    /**
     * @brief Handle time changes from EditorRegistry
     * 
     * Updates the scrollbar position when time changes come from other sources
     * (e.g., user double-clicks an interval in DataInspector).
     * 
     * @param position The new TimePosition
     */
    void _onEditorRegistryTimeChanged(TimePosition position);

    /**
     * @brief Handle DataManager state changes
     * 
     * When DataManager notifies of changes (e.g., data loaded, timeframes changed),
     * this method attempts to restore the current timeframe by:
     * 1) Trying to reget the timeframe for the existing key
     * 2) If that fails, getting the default timeframe from DataManager
     */
    void _onDataManagerChanged();

private slots:
    void Slider_Drag(int newPos);
    void Slider_Scroll(int newPos);
    void RewindButton();
    void FastForwardButton();
    void FrameSpinBoxChanged(int frameNumber);
signals:
    /**
     * @brief Emitted when user scrubs the timeline (includes TimeFrame pointer)
     * 
     * This is the preferred signal for time changes. It includes the TimePosition
     * which contains both the index and the TimeFrame pointer for clock identity.
     */
    void timeChanged(TimePosition position);

    /**
     * @brief Deprecated signal for backward compatibility
     * @deprecated Use timeChanged(TimePosition) instead
     */
    [[deprecated("Use timeChanged(TimePosition) instead")]]
    void timeChanged(int x);
};


#endif // TIMESCROLLBAR_H
