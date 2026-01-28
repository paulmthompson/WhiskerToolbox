#ifndef BINARY_LINE_SAVER_WIDGET_HPP
#define BINARY_LINE_SAVER_WIDGET_HPP

#include <QWidget>
#include <QString>
#include "nlohmann/json.hpp"

// Forward declaration
namespace Ui {
class BinaryLineSaver_Widget;
}

class BinaryLineSaver_Widget : public QWidget {
    Q_OBJECT
public:
    explicit BinaryLineSaver_Widget(QWidget *parent = nullptr);
    ~BinaryLineSaver_Widget() override;

signals:
    void saveBinaryRequested(QString format, nlohmann::json config);

private:
    Ui::BinaryLineSaver_Widget *ui;
};

#endif // BINARY_LINE_SAVER_WIDGET_HPP 