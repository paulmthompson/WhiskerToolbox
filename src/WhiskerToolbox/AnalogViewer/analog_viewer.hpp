//
// Created by wanglab on 4/15/2024.
//

#ifndef WHISKERTOOLBOX_ANALOG_VIEWER_HPP
#define WHISKERTOOLBOX_ANALOG_VIEWER_HPP

#include <QWidget>
#include <QMainWindow>

class DataManager;
class JKQTPGeoInfiniteLine;
class JKQTPPlotElement;
class JKQTPVerticalAxisBase;
class TimeScrollBar;

namespace Ui {
class Analog_Viewer;
}

class Analog_Viewer : public QMainWindow {
    Q_OBJECT
public:

    Analog_Viewer(std::shared_ptr<DataManager> data_manager, TimeScrollBar* time_scrollbar, QWidget *parent = 0);
    ~Analog_Viewer();

    void openWidget();

    void plotAnalog(std::string const & name);
    void plotDigital(std::string const & name);
    void removeGraph(std::string const & name);

protected:
    //void closeEvent(QCloseEvent *event);
    //void keyPressEvent(QKeyEvent *event);

private:
    std::shared_ptr<DataManager> _data_manager;
    TimeScrollBar* _time_scrollbar;
    Ui::Analog_Viewer *ui;

    enum class GraphType {
        analog,
        digital
    };
    struct GraphInfo {
        double height = 10.0;
        double offset = 0.0;
        JKQTPPlotElement* graph;
        JKQTPVerticalAxisBase* axis;
        bool show = true;
        bool show_axis = true;
        size_t ds_y_col;
        QColor color;
        GraphType type;

        GraphInfo() {}
    };
    std::map<std::string, GraphInfo> _graphs;

    JKQTPGeoInfiniteLine* _playhead;

    std::string _prev_graph_highlighted = ""; 
    int64_t _current_frame = 0;

    std::string _getSelectedGraphName();
    void _setZoom();
    void _scaleYAxis();

    std::vector<QColor> _palette = { 
        QColor("#BF4F51"), // red
        QColor("#007FFF"), // blue
        QColor("#ed9121"), // orange
        QColor("#50c878"), // green
        QColor("#8f00ff"), // violet
        QColor("#20b2aa"), // turquoise
        QColor("#ca1f7b"), // magenta
        QColor("#af6e4d") // brown
    };
    int _palette_idx = 0;
    QColor _nextColor();

public slots:
    void SetFrame(int i);

private slots:
    void GraphSetHeight();
    void GraphSetOffset();
    void GraphSetShow();
    void GraphSetShowAxis();
    void GraphDelete();
    void SetGraphEditor();
    void SetZoom();
    void ClickEvent(double x, double y, Qt::KeyboardModifiers modifiers, Qt::MouseButton button);
    void SnapFrameToCenter();

    void Alert();
};



#endif //WHISKERTOOLBOX_ANALOG_VIEWER_HPP
