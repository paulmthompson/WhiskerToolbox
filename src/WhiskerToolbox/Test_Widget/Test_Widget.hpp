#ifndef TEST_WIDGET_HPP
#define TEST_WIDGET_HPP

#include <QColor>
#include <QWidget>


namespace Ui {
class Test_Widget;
}

class Test_Widget : public QWidget {
    Q_OBJECT

public:
    explicit Test_Widget(QWidget* parent = nullptr);
    ~Test_Widget();

signals:

private slots:

private:
    Ui::Test_Widget* ui;

};

#endif// TEST_WIDGET_HPP
