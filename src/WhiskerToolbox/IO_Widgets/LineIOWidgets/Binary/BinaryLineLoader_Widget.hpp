#ifndef BINARY_LINE_LOADER_WIDGET_HPP
#define BINARY_LINE_LOADER_WIDGET_HPP

#include <QWidget>
#include <QString>

// Forward declaration of the UI class
namespace Ui {
class BinaryLineLoader_Widget;
}

class BinaryLineLoader_Widget : public QWidget {
    Q_OBJECT

public:
    explicit BinaryLineLoader_Widget(QWidget *parent = nullptr);
    ~BinaryLineLoader_Widget() override;

signals:
    void loadBinaryFileRequested(QString filepath);

private slots:
    void _onLoadBinaryFileButtonPressed();

private:
    Ui::BinaryLineLoader_Widget *ui;
};

#endif // BINARY_LINE_LOADER_WIDGET_HPP 