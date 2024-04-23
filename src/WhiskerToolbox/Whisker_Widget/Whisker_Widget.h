#ifndef WHISKER_WIDGET_H
#define WHISKER_WIDGET_H

#include <QWidget>

#include <memory>

#include "whiskertracker.h"
#include "Media_Window.h"

#include "DataManager.hpp"

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

class Whisker_Widget : public QWidget
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

    std::vector<Contact> _contact;
    int _contact_start;
    bool _contact_epoch;
    float _length_threshold;
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

};

#endif // WHISKER_WIDGET_H
