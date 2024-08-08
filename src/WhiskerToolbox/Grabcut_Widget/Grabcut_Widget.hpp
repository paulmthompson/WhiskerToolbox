#ifndef GRABCUT_WIDGET_HPP
#define GRABCUT_WIDGET_HPP

#include "Media_Window.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"
#include "DataManager.hpp"
#include "GrabCutTool.hpp"

#include <opencv2/imgcodecs.hpp>
#include <QMainWindow>
#include <QWidget>

#include <memory>

namespace Ui {
class Grabcut_Widget;
};

class Grabcut_Widget : public QMainWindow
{
    Q_OBJECT
public:

    Grabcut_Widget(Media_Window* scene, std::shared_ptr<DataManager> data_manager, TimeScrollBar* time_scrollbar, QWidget *parent = 0);
    void setup(cv::Mat img, int _frame_index);

    virtual ~Grabcut_Widget();

    void openWidget();
protected:
    void closeEvent(QCloseEvent *event);
    void keyPressEvent(QKeyEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

private:
    Media_Window * _scene;
    std::shared_ptr<DataManager> _data_manager;
    TimeScrollBar* _time_scrollbar;

    Ui::Grabcut_Widget *ui;

    int _frame_index;

    cv::Mat _img;
    GrabCutTool _tool;

    QPixmap _img_disp_pixmap;
    int _height, _width;

    void _updateDisplays();

    QPoint _scaleTransMouse(QPoint p);

private slots:
    void _setColor();
    void _setBrushThickness();
    void _grabcutIter();
    void _reset();
    void _saveMask();
    void _setMaskTransparency();
};

#endif // GRABCUT_WIDGET_HPP
