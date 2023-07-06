#ifndef WHISKER_WIDGET_H
#define WHISKER_WIDGET_H

#include <QWidget>

#include "ui_Whisker_Widget.h"

/*

This is our interface to using the Janelia whisker tracker.


*/

class Whisker_Widget : public QWidget, private Ui::Whisker_Widget
{
    Q_OBJECT
public:
    Whisker_Widget(QWidget *parent = 0) : QWidget(parent) {
        setupUi(this);

        createActions();

        //connect(ui->trace_button,SIGNAL(clicked()),this,SLOT(openConfig()));

    };

    void openWidget(); // Call
protected:
    void closeEvent(QCloseEvent *event);

private:
    void createActions();
    void openActions();
    void closeActions();

private slots:
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
