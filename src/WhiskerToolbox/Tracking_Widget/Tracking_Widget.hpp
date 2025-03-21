#ifndef BEHAVIORTOOLBOX_TRACKING_WIDGET_HPP
#define BEHAVIORTOOLBOX_TRACKING_WIDGET_HPP

#include <QMainWindow>

#include <memory>
#include <string>
#include <vector>

class DataManager;

namespace Ui {class Tracking_Widget;}

class Tracking_Widget : public QMainWindow {
Q_OBJECT

public:

    Tracking_Widget(std::shared_ptr<DataManager> data_manager,
                    QWidget *parent = 0);

    virtual ~Tracking_Widget();

    void openWidget(); // Call
public slots:

    void LoadFrame(int frame_id);

protected:
    void closeEvent(QCloseEvent *event);

    //void keyPressEvent(QKeyEvent *event);

private:
    std::shared_ptr<DataManager> _data_manager;
    Ui::Tracking_Widget *ui;

    int _previous_frame {0};

    std::string _current_tracking_key;

    void _propagateLabel(int frame_id);

private slots:

};

#endif //BEHAVIORTOOLBOX_TRACKING_WIDGET_HPP
