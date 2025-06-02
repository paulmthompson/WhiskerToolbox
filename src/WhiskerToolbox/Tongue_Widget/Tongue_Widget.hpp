#ifndef TONGUE_WIDGET_HPP
#define TONGUE_WIDGET_HPP

#include <QMainWindow>
#include <QPointer>

#include <memory>

class DataManager;
class Grabcut_Widget;

namespace Ui {
class Tongue_Widget;
}

class Tongue_Widget : public QMainWindow
{
    Q_OBJECT
public:

    Tongue_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent = nullptr);

    ~Tongue_Widget() override;

    void openWidget(); //
protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    std::shared_ptr<DataManager> _data_manager;
    QPointer<Grabcut_Widget> _grabcut_widget;

    Ui::Tongue_Widget *ui;

    std::vector<int> drawn;

private slots:
    void _loadImgTongueMasks();
    void _startGrabCut();
    void _exportMasks();
};



#endif // TONGUE_WIDGET_HPP
