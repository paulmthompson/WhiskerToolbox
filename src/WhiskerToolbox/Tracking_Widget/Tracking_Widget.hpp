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

    explicit Tracking_Widget(std::shared_ptr<DataManager> data_manager,
                    QWidget *parent = nullptr);

    ~Tracking_Widget() override;

    void openWidget(); // Call
public slots:

    void LoadFrame(int frame_id);

protected:
    void closeEvent(QCloseEvent *event) override;

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
