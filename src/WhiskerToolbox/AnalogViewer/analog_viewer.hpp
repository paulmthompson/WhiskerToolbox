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

    void plotLine(std::string name);
    void removeGraph(std::string name);

protected:
    //void closeEvent(QCloseEvent *event);
    //void keyPressEvent(QKeyEvent *event);

private:
    Media_Window * _scene;
    std::shared_ptr<DataManager> _data_manager;
    TimeScrollBar* _time_scrollbar;

    Ui::Analog_Viewer *ui;

    struct PlotElementInfo {
        double mult = 1.0;
        double add = 0.0;
        JKQTPPlotElement* element = nullptr;
        size_t ds_y_col;

        PlotElementInfo() {}
    };
    std::map<std::string, PlotElementInfo> _plot_elements;

    void _setZoom(int i);

    void _elementApplyLintrans(std::string name);
public slots:
    void SetFrame(int i);

private slots:
    void ElementSetLintrans();
    void ResetLineEditor();
    void SetZoom(int i);
};



#endif //WHISKERTOOLBOX_ANALOG_VIEWER_HPP
