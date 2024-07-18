//
// Created by wanglab on 4/15/2024.
//

#ifndef WHISKERTOOLBOX_ANALOG_VIEWER_HPP
#define WHISKERTOOLBOX_ANALOG_VIEWER_HPP

#include <QWidget>
#include <QMainWindow>

#include "jkqtplotter/jkqtplotter.h"

#include "Media_Window.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"
#include "DataManager.hpp"

namespace Ui {
class Analog_Viewer;
}

class Analog_Viewer : public QMainWindow {
    Q_OBJECT
public:

    Analog_Viewer(Media_Window* scene, std::shared_ptr<DataManager> data_manager, TimeScrollBar* time_scrollbar, QWidget *parent = 0);
    ~Analog_Viewer();

    void openWidget();

    void plotLine(std::string name, std::vector<double>& data);
    void removeGraph(std::string name);

protected:
    //void closeEvent(QCloseEvent *event);
    //void keyPressEvent(QKeyEvent *event);

private:
    Media_Window * _scene;
    std::shared_ptr<DataManager> _data_manager;
    TimeScrollBar* _time_scrollbar;

    Ui::Analog_Viewer *ui;

    std::map<std::string, JKQTPPlotElement*> _plot_elements;


public slots:
    void SetFrame(int i);

private slots:

};



#endif //WHISKERTOOLBOX_ANALOG_VIEWER_HPP
