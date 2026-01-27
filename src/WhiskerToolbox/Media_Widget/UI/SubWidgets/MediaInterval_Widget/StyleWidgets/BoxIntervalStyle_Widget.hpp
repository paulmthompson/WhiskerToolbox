#ifndef BOXINTERVALSTYLE_WIDGET_HPP
#define BOXINTERVALSTYLE_WIDGET_HPP

#include <QWidget>
#include <memory>

namespace Ui {
class BoxIntervalStyle_Widget;
}

class DataManager;
class Media_Window;
struct DigitalIntervalDisplayOptions;

class BoxIntervalStyle_Widget : public QWidget {
    Q_OBJECT

public:
    explicit BoxIntervalStyle_Widget(QWidget *parent = nullptr);
    ~BoxIntervalStyle_Widget();

    void setActiveKey(std::string const & key);
    void setScene(Media_Window * scene);
    void updateFromConfig(DigitalIntervalDisplayOptions const * config);

signals:
    void configChanged();

private slots:
    void _setBoxSize(int size);
    void _setFrameRange(int range);
    void _setLocation(int location_index);

private:
    Ui::BoxIntervalStyle_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window * _scene;
    std::string _active_key;
};

#endif // BOXINTERVALSTYLE_WIDGET_HPP 