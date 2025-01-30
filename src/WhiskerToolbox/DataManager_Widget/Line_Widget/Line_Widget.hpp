#ifndef LINE_WIDGET_HPP
#define LINE_WIDGET_HPP

#include <QWidget>
#include <memory>
#include <string>

namespace Ui { class Line_Widget; }

class DataManager;

class Line_Widget : public QWidget
{
    Q_OBJECT
public:
    explicit Line_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent = nullptr);
    ~Line_Widget();

    void openWidget(); // Call to open the widget

private:
    Ui::Line_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;

private slots:
               // Add any slots needed for handling user interactions
};

#endif // LINE_WIDGET_HPP
