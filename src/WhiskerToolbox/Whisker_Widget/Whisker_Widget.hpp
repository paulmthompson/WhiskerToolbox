#ifndef WHISKER_WIDGET_HPP
#define WHISKER_WIDGET_HPP

#include "ImageSize/ImageSize.hpp"// for ImageSize
#include "Lines/lines.hpp"
#include "Points/points.hpp"// for Point2D

#include <QMainWindow>
#include <QPointer>

#include <memory>
#include <string>
#include <vector>

class DataManager;
class Janelia_Config;

namespace Ui {
class Whisker_Widget;
}
namespace whisker {
class WhiskerTracker;
struct Line2D;
}// namespace whisker

namespace dl {
class SCM;
}

/*

This is our interface to using the Janelia whisker tracker.


*/

class Whisker_Widget : public QMainWindow {
    Q_OBJECT
public:
    Whisker_Widget(std::shared_ptr<DataManager> data_manager,
                   QWidget * parent = nullptr);

    ~Whisker_Widget() override;

    void openWidget();// Call
public slots:
    void LoadFrame(int frame_id);

protected:
    void closeEvent(QCloseEvent * event) override;
    void keyPressEvent(QKeyEvent * event) override;

private:
    std::shared_ptr<whisker::WhiskerTracker> _wt;
    std::shared_ptr<DataManager> _data_manager;

    int _selected_whisker{0};

    float _linking_tolerance{20.0f};

    QPointer<Janelia_Config> _janelia_config_widget;

    enum Face_Orientation {
        Facing_Top,
        Facing_Bottom,
        Facing_Left,
        Facing_Right
    };

    Face_Orientation _face_orientation{Facing_Top};

    int _num_whisker_to_track{0};

    int _clip_length{0};

    int _current_whisker{0};

    bool _auto_dl{false};
    
    std::string _current_whisker_pad_key; // Current selected PointData key for whisker pad
    Point2D<float> _current_whisker_pad_point{0.0f, 0.0f}; // Current whisker pad position

    /*
     * DL Model for whisker tracing
     */
    std::unique_ptr<dl::SCM> dl_model{nullptr};

    Ui::Whisker_Widget * ui;

    void _createNewWhisker(std::string const & whisker_group_name, int whisker_id);

    void _traceWhiskers(std::vector<uint8_t> image, ImageSize image_size);
    void _traceWhiskersDL(std::vector<uint8_t> image, ImageSize image_size);

    // Whisker pad management methods
    void _populateWhiskerPadCombo();
    void _updateWhiskerPadFromSelection();
    void _updateWhiskerPadLabel();
    void _createNewWhiskerPad();

private slots:
    void _traceButton();
    void _dlTraceButton();
    void _dlAddMemoryButton();

    void _loadJaneliaWhiskers();

    void _changeWhiskerLengthThreshold(double new_threshold);

    void _selectFaceOrientation(int index);

    void _selectNumWhiskersToTrack(int n_whiskers);

    void _openJaneliaConfig();

    void _changeWhiskerClip(int clip_dist);

    void _selectWhisker(int whisker_num);
    
    // New slots for whisker pad management
    void _onWhiskerPadComboChanged(const QString& text);
    void _onWhiskerPadFrameChanged(int frame);

};

void order_whiskers_by_position(DataManager * dm, std::string const & whisker_group_name, int num_whiskers_to_track, int current_time, float similarity_threshold);

void add_whiskers_to_data_manager(
        DataManager * dm,
        std::vector<Line2D> & whiskers,
        std::string const & whisker_group_name,
        int num_whisker_to_track,
        int current_time,
        float similarity_threshold);

void clip_whisker(Line2D & line, int clip_length);


#endif// WHISKER_WIDGET_HPP
