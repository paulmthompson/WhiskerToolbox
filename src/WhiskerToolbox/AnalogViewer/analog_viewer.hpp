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

    struct GraphInfo {
        double mult = 1.0;
        double add = 0.0;
        JKQTPPlotElement* graph = nullptr;
        bool show = true;
        size_t ds_y_col;

        GraphInfo() {}
    };
    std::map<std::string, GraphInfo> _graphs;

    void _setZoom();

    void _elementApplyLintrans(std::string name);

    std::string _prev_element = ""; 

    int64_t _current_frame = 0;
public slots:
    void SetFrame(int i);

private slots:
    void ElementSetLintrans();
    void SetPlotEditor();
    void SetZoom();
};



#endif //WHISKERTOOLBOX_ANALOG_VIEWER_HPP
