#ifndef BEHAVIORTOOLBOX_TRACKING_WIDGET_HPP
#define BEHAVIORTOOLBOX_TRACKING_WIDGET_HPP

#include <QMainWindow>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class DataManager;
class MainWindow;
class Media_Window;
class QGraphicsScene;
class TimeScrollBar;

namespace Ui {class Tracking_Widget;}

class Tracking_Widget : public QMainWindow {
Q_OBJECT

public:

    Tracking_Widget(Media_Window *scene,
                    std::shared_ptr<DataManager> data_manager,
                    TimeScrollBar *time_scrollbar,
                    MainWindow *main_window,
                    QWidget *parent = 0);

    virtual ~Tracking_Widget();

    void openWidget(); // Call
public slots:

    void LoadFrame(int frame_id);

protected:
    void closeEvent(QCloseEvent *event);

    //void keyPressEvent(QKeyEvent *event);

private:
    Media_Window * _scene;
    std::shared_ptr<DataManager> _data_manager;
    TimeScrollBar* _time_scrollbar;
    MainWindow* _main_window;
    Ui::Tracking_Widget *ui;

    QGraphicsScene* _selected_scene;

    enum Selection_Type {Tracking_Select};

    Tracking_Widget::Selection_Type _selection_mode {Tracking_Select};

    int _highlighted_row {-1};
    int _previous_frame {0};

    std::filesystem::path _output_path;

    void _buildContactTable();
    void _propagateLabel(int frame_id);

private slots:
    void _clickedInVideo(qreal x,qreal y);
    void _tableClicked(int row, int column);
    void _loadKeypointCSV();
    void _saveKeypointCSV();
    void _changeOutputDir();
};

#endif //BEHAVIORTOOLBOX_TRACKING_WIDGET_HPP
