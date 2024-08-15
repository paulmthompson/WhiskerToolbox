#ifndef WHISKER_WIDGET_HPP
#define WHISKER_WIDGET_HPP

#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"  // for Point2D

#include <QMainWindow>
#include <QPointer>

#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>

class DataManager;
class Janelia_Config;
class MainWindow;
class Media_Window;
class TimeScrollBar;

namespace Ui {class Whisker_Widget;}
namespace whisker {class WhiskerTracker; struct Line2D;}



/*

This is our interface to using the Janelia whisker tracker.


*/

class Whisker_Widget : public QMainWindow
{
    Q_OBJECT
public:

    Whisker_Widget(Media_Window* scene,
                   std::shared_ptr<DataManager> data_manager,
                   TimeScrollBar* time_scrollbar,
                   MainWindow* main_window,
                   QWidget *parent = 0);

    virtual ~Whisker_Widget();

    void openWidget(); // Call
public slots:
    void LoadFrame(int frame_id);
protected:
    void closeEvent(QCloseEvent *event);
    void keyPressEvent(QKeyEvent *event);

private:
    std::shared_ptr<whisker::WhiskerTracker> _wt;
    Media_Window * _scene;
    std::shared_ptr<DataManager> _data_manager;
    TimeScrollBar* _time_scrollbar;
    MainWindow* _main_window;

    int _selected_whisker {0};

    enum Selection_Type {Whisker_Select,
                          Whisker_Pad_Select};

    Whisker_Widget::Selection_Type _selection_mode {Whisker_Select};

    QPointer<Janelia_Config> _janelia_config_widget;

    enum Face_Orientation {
        Facing_Top,
        Facing_Bottom,
        Facing_Left,
        Facing_Right
    };

    Face_Orientation _face_orientation {Facing_Top};

    int _num_whisker_to_track {0};

    bool _save_by_frame_name {false};

    std::set<int> _tracked_frame_ids {};

    std::filesystem::path _output_path;

    Ui::Whisker_Widget *ui;

    void _drawWhiskers();
    void _addWhiskersToData(std::vector<Line2D> & whiskers);
    void _createNewWhisker(std::string const & whisker_name, int const whisker_id);
    void _orderWhiskersByPosition();
    std::vector<Point2D<float>> _getWhiskerBasePositions();

    void _saveImage(std::string const& folder);
    std::string _getImageSaveName(int const frame_id);
    std::string _getWhiskerSaveName(int const frame_id);

    void _loadSingleHDF5WhiskerMask(std::string const & filename);
    void _loadSingleHDF5WhiskerLine(std::string const & filename);
    std::vector<int> _loadCSVWhiskerFromDir(std::string const & dir_name);

    void _addNewTrackedWhisker(int const index);
    void _addNewTrackedWhisker(std::vector<int> const & indexes);

private slots:
    void _traceButton();
    void _saveImageButton();
    void _saveFaceMask();
    void _loadFaceMask();

    void _loadJaneliaWhiskers();

    void _loadHDF5WhiskerMask();
    void _loadHDF5WhiskerMasksFromDir();

    void _loadHDF5WhiskerLine();
    void _loadHDF5WhiskerLinesFromDir();

    void _loadSingleCSVWhisker();
    void _loadMultiCSVWhiskers();

    void _selectWhiskerPad();
    void _changeWhiskerLengthThreshold(double new_threshold);

    void _selectFaceOrientation(int index);

    void _selectNumWhiskersToTrack(int n_whiskers);

    void _clickedInVideo(qreal x,qreal y);

    void _exportImageCSV();
    void _saveWhiskerAsCSV(std::string const& folder, std::vector<Point2D<float>> const& whisker);

    void _openJaneliaConfig();
    void _openContactWidget();

    void _setMaskAlpha(int);

    void _skipToTrackedFrame(int index);

    void _maskDilation(int dilation_size);
    void _maskDilationExtended(int dilation_size);

    void _loadKeypointCSV();

    void _changeOutputDir();

};

void _printBasePositionOrder(const std::vector<Point2D<float>> &base_positions);
bool _checkWhiskerNumMatchesExportNum(DataManager* dm, int const num_whiskers_to_export, std::string const & whisker_prefix);

#endif // WHISKER_WIDGET_HPP
