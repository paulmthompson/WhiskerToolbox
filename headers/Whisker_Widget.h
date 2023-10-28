#ifndef WHISKER_WIDGET_H
#define WHISKER_WIDGET_H

#include <QWidget>

#include <memory>

#include "whiskertracker.h"
#include "Media_Window.h"

#include "ui_Whisker_Widget.h"

#include "TimeFrame.h"

/*

This is our interface to using the Janelia whisker tracker.


*/

class Whisker_Widget : public QWidget, private Ui::Whisker_Widget
{
    Q_OBJECT
public:
    Whisker_Widget(Media_Window* scene, std::shared_ptr<TimeFrame> time, QWidget *parent = 0) : QWidget(parent) {
        setupUi(this);

        this->scene = scene;

        this->time = time;

        createActions();

        this->wt = std::make_unique<WhiskerTracker>();
        this->selected_whisker = 0;
        this->selection_mode = Whisker_Select;
        //connect(ui->trace_button,SIGNAL(clicked()),this,SLOT(openConfig()));

    };

    void openWidget(); // Call
protected:
    void closeEvent(QCloseEvent *event);

private:
    void createActions();
    void openActions();
    void closeActions();
    void DrawWhiskers();

    std::unique_ptr<WhiskerTracker> wt;
    Media_Window * scene;
    std::shared_ptr<TimeFrame> time;
    int selected_whisker;
    enum Selection_Type {Whisker_Select,
                          Whisker_Pad_Select};
    Whisker_Widget::Selection_Type selection_mode;

private slots:
    void TraceButton();
    void SaveImageButton();
    void ClickedInVideo(qreal x,qreal y);
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
