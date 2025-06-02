#ifndef GRABCUT_WIDGET_HPP
#define GRABCUT_WIDGET_HPP

#include "GrabCutTool.hpp"

#include <QMainWindow>
#include <QWidget>
#include <opencv2/imgcodecs.hpp>

#include <memory>

class DataManager;
namespace Ui {
class Grabcut_Widget;
};

class Grabcut_Widget : public QMainWindow {
    Q_OBJECT
public:
    Grabcut_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    void setup(cv::Mat img, int _frame_index);

    ~Grabcut_Widget() override;

    void openWidget();

protected:
    void closeEvent(QCloseEvent * event) override;
    void keyPressEvent(QKeyEvent * event) override;
    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;

private:
    std::shared_ptr<DataManager> _data_manager;

    Ui::Grabcut_Widget * ui;

    int _frame_index = 0;

    cv::Mat _img;
    GrabCutTool _tool;

    QPixmap _img_disp_pixmap;
    int _height = 0;
    int _width = 0;

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

#endif// GRABCUT_WIDGET_HPP
