#ifndef BORDERINTERVALSTYLE_WIDGET_HPP
#define BORDERINTERVALSTYLE_WIDGET_HPP

#include <QWidget>
#include <memory>

namespace Ui {
class BorderIntervalStyle_Widget;
}

class DataManager;
class Media_Window;
struct DigitalIntervalDisplayOptions;

class BorderIntervalStyle_Widget : public QWidget {
    Q_OBJECT

public:
    explicit BorderIntervalStyle_Widget(QWidget *parent = nullptr);
    ~BorderIntervalStyle_Widget();

    void setActiveKey(std::string const & key);
    void setScene(Media_Window * scene);
    void updateFromConfig(DigitalIntervalDisplayOptions const * config);

signals:
    void configChanged();

private slots:
    void _setBorderThickness(int thickness);

private:
    Ui::BorderIntervalStyle_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window * _scene;
    std::string _active_key;
};

#endif // BORDERINTERVALSTYLE_WIDGET_HPP 