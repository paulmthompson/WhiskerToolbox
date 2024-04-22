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
    void _createActions();
    void _openActions();
    void _closeActions();
    void _DrawWhiskers();
    void _addWhiskersToData();

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

    Ui::Whisker_Widget *ui;

private slots:
    void _TraceButton();
    void _SaveImageButton();
    void _SaveWhiskerMaskButton();

    void _ContactButton();
    void _SaveContact();
    void _LoadContact();
    void _LoadJaneliaWhiskers();

    void _ClickedInVideo(qreal x,qreal y);
    /*
    void openConfig() {
        config_win->updateValues();
        config_win->show();
    };
    void updateValues() {
        std::cout << "Y Max: " << c_opt->y_max << std::endl;
        std::cout << "Y Min: " << c_opt->y_min << std::endl;

        //Replot
    };
*/
};

#endif // WHISKER_WIDGET_H
