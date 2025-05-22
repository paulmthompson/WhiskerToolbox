#ifndef WHISKER_WIDGET_HPP
#define WHISKER_WIDGET_HPP

#include "ImageSize/ImageSize.hpp"// for ImageSize
#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"// for Point2D

#include <QMainWindow>
#include <QPointer>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class DataManager;
class Janelia_Config;
class MainWindow;
class Media_Window;
class TimeScrollBar;

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
                   TimeScrollBar * time_scrollbar,
                   MainWindow * main_window,
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
    TimeScrollBar * _time_scrollbar;
    MainWindow * _main_window;

    int _selected_whisker{0};

    float _linking_tolerance{20.0f};

    enum Selection_Type { Whisker_Select,
                          Whisker_Pad_Select,
                          Magic_Eraser};

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

    bool _save_by_frame_name{false};

    std::filesystem::path _output_path;

    int _clip_length{0};

    int _current_whisker{0};

    bool _auto_dl{false};

    Ui::Whisker_Widget * ui;

    std::unique_ptr<dl::SCM> dl_model{nullptr};

    void _createNewWhisker(std::string const & whisker_group_name, int whisker_id);

    void _saveImage(std::string const & folder);
    std::string _getWhiskerSaveName(int frame_id);

    std::vector<int> _loadCSVWhiskerFromDir(std::string const & dir_name, std::string const & whisker_group_name);

    void _addNewTrackedWhisker(int index);
    void _addNewTrackedWhisker(std::vector<int> const & indexes);

    void _traceWhiskers(std::vector<uint8_t> image, ImageSize image_size);
    void _traceWhiskersDL(std::vector<uint8_t> image, ImageSize image_size);

    void _addDrawingCallback(std::string data_name);


private slots:
    void _traceButton();
    void _dlTraceButton();
    void _dlAddMemoryButton();
    void _saveFaceMask();
    void _loadFaceMask();

    void _loadJaneliaWhiskers();

    void _loadSingleCSVWhisker();
    void _loadMultiCSVWhiskers();

    void _selectWhiskerPad();
    void _changeWhiskerLengthThreshold(double new_threshold);

    void _selectFaceOrientation(int index);

    void _selectNumWhiskersToTrack(int n_whiskers);

    void _clickedInVideo(qreal x, qreal y);

    void _exportImageCSV();
    void _saveWhiskerAsCSV(std::string const & folder, std::vector<Point2D<float>> const & whisker);

    void _openJaneliaConfig();
    void _openContactWidget();

    void _maskDilation(int dilation_size);
    void _maskDilationExtended(int dilation_size);

    void _changeOutputDir();

    void _changeWhiskerClip(int clip_dist);

    void _magicEraserButton();

    void _drawingFinished();

    void _selectWhisker(int whisker_num);

    void _deleteWhisker();

    void _saveWhiskersAsCSV();
    void _loadMultiFrameCSV();

    void _saveWhiskersAsBinary();

    void _exportAllTracked();
};

void order_whiskers_by_position(DataManager * dm, std::string const & whisker_group_name, int num_whiskers_to_track, int current_time, float similarity_threshold);

std::vector<int> load_csv_lines_into_data_manager(DataManager * dm, std::string const & dir_name, std::string const & line_key);

bool check_whisker_num_matches_export_num(DataManager * dm, int num_whiskers_to_export, std::string const & whisker_group_name);

void add_whiskers_to_data_manager(
        DataManager * dm,
        std::vector<Line2D> & whiskers,
        std::string const & whisker_group_name,
        int num_whisker_to_track,
        int current_time,
        float similarity_threshold);

void clip_whisker(Line2D & line, int clip_length);

std::string generate_color();

std::string get_whisker_color(int whisker_index);

#endif// WHISKER_WIDGET_HPP
