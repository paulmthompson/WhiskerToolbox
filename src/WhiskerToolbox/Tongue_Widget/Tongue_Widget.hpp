#ifndef TONGUE_WIDGET_HPP
#define TONGUE_WIDGET_HPP

#include <QMainWindow>
#include <QPointer>

#include <memory>

class DataManager;
class Grabcut_Widget;
class TongueWidgetState;

namespace Ui {
class Tongue_Widget;
}

class Tongue_Widget : public QMainWindow
{
    Q_OBJECT
public:

    Tongue_Widget(std::shared_ptr<DataManager> data_manager,
                  std::shared_ptr<TongueWidgetState> state,
                  QWidget *parent = nullptr);

    ~Tongue_Widget() override;

    void openWidget();

    /**
     * @brief Get the widget's state object
     * @return Shared pointer to TongueWidgetState
     */
    [[nodiscard]] std::shared_ptr<TongueWidgetState> state() const { return _state; }

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    std::shared_ptr<DataManager> _data_manager;
    std::shared_ptr<TongueWidgetState> _state;
    QPointer<Grabcut_Widget> _grabcut_widget;

    Ui::Tongue_Widget *ui;

private slots:
    void _startGrabCut();
};



#endif // TONGUE_WIDGET_HPP
