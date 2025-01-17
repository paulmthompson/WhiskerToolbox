#ifndef TONGUE_WIDGET_HPP
#define TONGUE_WIDGET_HPP

#include <QMainWindow>
#include <QPointer>

#include <memory>

class DataManager;
class Grabcut_Widget;
class Media_Window;
class TimeScrollBar;

namespace Ui {
class Tongue_Widget;
}

class Tongue_Widget : public QMainWindow
{
    Q_OBJECT
public:

    Tongue_Widget(Media_Window* scene, std::shared_ptr<DataManager> data_manager, TimeScrollBar* time_scrollbar, QWidget *parent = 0);

    virtual ~Tongue_Widget();

    void openWidget(); //
protected:
    void closeEvent(QCloseEvent *event);
    void keyPressEvent(QKeyEvent *event);

private:
    Media_Window * _scene;
    std::shared_ptr<DataManager> _data_manager;
    QPointer<Grabcut_Widget> _grabcut_widget;
    TimeScrollBar* _time_scrollbar;

    Ui::Tongue_Widget *ui;

    std::vector<int> drawn;

private slots:
    void _loadHDF5TongueMasks();
    void _loadImgTongueMasks();
    void _loadCSVJawKeypoints();
    void _startGrabCut();
    void _exportMasks();
};



#endif // TONGUE_WIDGET_HPP
