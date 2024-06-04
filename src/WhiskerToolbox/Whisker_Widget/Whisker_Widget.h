#ifndef WHISKER_WIDGET_H
#define WHISKER_WIDGET_H

#include <QMainWindow>
#include <QPointer>

#include <memory>

#include "whiskertracker.h"
#include "Media_Window.h"
#include "janelia_config.hpp"
#include "contact_widget.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"


#include "DataManager.hpp"

/*
 *
 * Currently I can export all symbols on win32, but I think
 * because I use dllopt in ffmepg_wrapper, it does not actually
 * od that anymore with clang, and I needt o manually define these
 *
 */
#if defined _WIN32 || defined __CYGWIN__
    #define WHISKER_WIDGET_DLLOPT
    //#define WHISKER_WIDGET_DLLOPT Q_DECL_EXPORT
#else
    #define WHISKER_WIDGET_DLLOPT __attribute__((visibility("default")))
#endif


namespace Ui {
    class Whisker_Widget;
}

/*

This is our interface to using the Janelia whisker tracker.


*/

class WHISKER_WIDGET_DLLOPT Whisker_Widget : public QMainWindow
{
    Q_OBJECT
public:

    Whisker_Widget(Media_Window* scene, std::shared_ptr<DataManager> data_manager, TimeScrollBar* time_scrollbar, QWidget *parent = 0);

    virtual ~Whisker_Widget();

    void openWidget(); // Call
public slots:
    void LoadFrame(int frame_id);
protected:
    void closeEvent(QCloseEvent *event);

private:
    std::shared_ptr<WhiskerTracker> _wt;
    Media_Window * _scene;
    std::shared_ptr<DataManager> _data_manager;
    int _selected_whisker;
    enum Selection_Type {Whisker_Select,
                          Whisker_Pad_Select};
    Whisker_Widget::Selection_Type _selection_mode;

    QPointer<Janelia_Config> _janelia_config_widget;
    QPointer<Contact_Widget> _contact_widget;

    std::tuple<float,float> _whisker_pad;

    enum Face_Orientation {
        Facing_Top,
        Facing_Bottom,
        Facing_Left,
        Facing_Right
    };

    Face_Orientation _face_orientation;

    int _num_whisker_to_track;

    bool _save_by_frame_name;

    Ui::Whisker_Widget *ui;

    void _drawWhiskers();
    void _addWhiskersToData();
    void _orderWhiskersByPosition();
    std::vector<Point2D> _getWhiskerBasePositions();

    void _saveImage(const std::string& folder);
    std::string _getImageSaveName(int frame_id);
    std::string _getWhiskerSaveName(int frame_id);

private slots:
    void _traceButton();
    void _saveImageButton();
    void _saveWhiskerMaskButton();

    void _loadJaneliaWhiskers();
    void _loadHDF5Whiskers();

    void _selectWhiskerPad();
    void _changeWhiskerLengthThreshold(double new_threshold);

    void _selectFaceOrientation(int index);

    void _selectNumWhiskersToTrack(int n_whiskers);

    void _clickedInVideo(qreal x,qreal y);

    void _exportImageCSV();
    void _saveWhiskerAsCSV(const std::string& folder, const std::vector<Point2D>& whisker);

    void _openJaneliaConfig();
    void _openContactWidget();

    void _setMaskAlpha(int);

};

void _printBasePositionOrder(const std::vector<Point2D> &base_positions);
std::string remove_extension(const std::string& filename);
std::string pad_frame_id(int frame_id, int pad_digits);

#endif // WHISKER_WIDGET_H
