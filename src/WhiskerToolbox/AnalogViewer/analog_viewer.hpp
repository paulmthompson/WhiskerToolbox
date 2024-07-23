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

    void plotAnalog(std::string name);
    void plotDigital(std::string name);
    void removeGraph(std::string name);

protected:
    //void closeEvent(QCloseEvent *event);
    //void keyPressEvent(QKeyEvent *event);

private:
    Media_Window * _scene;
    std::shared_ptr<DataManager> _data_manager;
    TimeScrollBar* _time_scrollbar;

    Ui::Analog_Viewer *ui;

    enum class GraphType {
        analog,
        digital
    };
    struct GraphInfo {
        double mult = 1.0;
        double add = 0.0;
        JKQTPPlotElement* graph;
        bool show = true;
        size_t ds_y_col;
        GraphType type;

        GraphInfo() {}
    };
    std::map<std::string, GraphInfo> _graphs;

    void _setZoom();

    void _graphApplyLintrans(std::string name);

    std::string _prev_graph_highlighted = ""; 

    int64_t _current_frame = 0;

    std::string _getSelectedGraphName();
public slots:
    void SetFrame(int i);

private slots:
    void GraphSetLintrans();
    void GraphSetShow();
    void GraphDelete();
    void SetPlotEditor();
    void SetZoom();
    void ClickEvent(double x, double y, Qt::KeyboardModifiers modifiers, Qt::MouseButton button);
};



#endif //WHISKERTOOLBOX_ANALOG_VIEWER_HPP
