#ifndef WHISKER_WIDGET_H
#define WHISKER_WIDGET_H

#include <QWidget>
#include <QPointer>

#include <memory>

#include "whiskertracker.h"
#include "Media_Window.h"
#include "janelia_config.hpp"

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

enum class Contact : int {
    Contact = 1,
    NoContact = 0
};

class WHISKER_WIDGET_DLLOPT Whisker_Widget : public QWidget
{
    Q_OBJECT
public:

    Whisker_Widget(Media_Window* scene, std::shared_ptr<DataManager> data_manager, QWidget *parent = 0);

    virtual ~Whisker_Widget();

    void openWidget(); // Call
protected:
    void closeEvent(QCloseEvent *event);

private:
    std::unique_ptr<WhiskerTracker> _wt;
    Media_Window * _scene;
    std::shared_ptr<DataManager> _data_manager;
    int _selected_whisker;
    enum Selection_Type {Whisker_Select,
                          Whisker_Pad_Select};
    Whisker_Widget::Selection_Type _selection_mode;

    QPointer<Janelia_Config> _janelia_config_widget;

    std::vector<Contact> _contact;
    int _contact_start;
    bool _contact_epoch;
    std::tuple<float,float> _whisker_pad;

    enum Face_Orientation {
        Facing_Top,
        Facing_Bottom,
        Facing_Left,
        Facing_Right
    };

    Face_Orientation _face_orientation;

    int _num_whisker_to_track;

    Ui::Whisker_Widget *ui;

    void _drawWhiskers();
    void _addWhiskersToData();
    void _orderWhiskersByPosition();
    std::vector<Point2D> _getWhiskerBasePositions();
    void _saveImage(const std::string folder);

private slots:
    void _traceButton();
    void _saveImageButton();
    void _saveWhiskerMaskButton();

    void _contactButton();
    void _saveContact();
    void _loadContact();
    void _loadJaneliaWhiskers();

    void _selectWhiskerPad();
    void _changeWhiskerLengthThreshold(double new_threshold);

    void _selectFaceOrientation(int index);

    void _selectNumWhiskersToTrack(int n_whiskers);

    void _clickedInVideo(qreal x,qreal y);

    void _exportImageCSV();
    void _saveWhiskerAsCSV(const std::string& folder, const std::vector<Point2D>& whisker);

    void _openJaneliaConfig();
};

#endif // WHISKER_WIDGET_H
