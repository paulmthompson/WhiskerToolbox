#ifndef BINARY_LINE_SAVER_WIDGET_HPP
#define BINARY_LINE_SAVER_WIDGET_HPP

#include <QWidget>
#include "DataManager/Lines/IO/Binary/Line_Data_Binary.hpp" // For BinaryLineSaverOptions

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
    void saveBinaryRequested(BinaryLineSaverOptions options);

private:
    Ui::BinaryLineSaver_Widget *ui;
};

#endif // BINARY_LINE_SAVER_WIDGET_HPP 