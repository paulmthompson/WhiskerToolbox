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
class MainWindow;
class Media_Window;

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
    Whisker_Widget(Media_Window * scene,
                   std::shared_ptr<DataManager> data_manager,
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
    Media_Window * _scene;
    std::shared_ptr<DataManager> _data_manager;

    int _selected_whisker{0};

    float _linking_tolerance{20.0f};

    enum Selection_Type { Whisker_Select,
                          Whisker_Pad_Select};

    Whisker_Widget::Selection_Type _selection_mode{Whisker_Select};

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

    Ui::Whisker_Widget * ui;

    std::unique_ptr<dl::SCM> dl_model{nullptr};

    void _createNewWhisker(std::string const & whisker_group_name, int whisker_id);

    void _traceWhiskers(std::vector<uint8_t> image, ImageSize image_size);
    void _traceWhiskersDL(std::vector<uint8_t> image, ImageSize image_size);

private slots:
    void _traceButton();
    void _dlTraceButton();
    void _dlAddMemoryButton();

    void _loadJaneliaWhiskers();

    void _selectWhiskerPad();
    void _changeWhiskerLengthThreshold(double new_threshold);

    void _selectFaceOrientation(int index);

    void _selectNumWhiskersToTrack(int n_whiskers);

    void _clickedInVideo(qreal x, qreal y);

    void _openJaneliaConfig();

    void _changeWhiskerClip(int clip_dist);

    void _selectWhisker(int whisker_num);

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
