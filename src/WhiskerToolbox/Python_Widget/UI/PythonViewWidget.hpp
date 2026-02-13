#ifndef PYTHON_VIEW_WIDGET_HPP
#define PYTHON_VIEW_WIDGET_HPP

/**
 * @file PythonViewWidget.hpp
 * @brief Main view widget for the Python integration
 *
 * PythonViewWidget is the primary view for the Python Widget.
 * It will contain the Python script editor and output display.
 *
 * @see PythonWidgetState for the shared state class
 * @see PythonPropertiesWidget for the properties panel
 */

#include <QWidget>

#include <memory>

class PythonWidgetState;

namespace Ui {
class PythonViewWidget;
}

class PythonViewWidget : public QWidget {
    Q_OBJECT

public:
    explicit PythonViewWidget(std::shared_ptr<PythonWidgetState> state,
                              QWidget * parent = nullptr);
    ~PythonViewWidget() override;

    [[nodiscard]] std::shared_ptr<PythonWidgetState> getState() const { return _state; }

private:
    std::unique_ptr<Ui::PythonViewWidget> ui;
    std::shared_ptr<PythonWidgetState> _state;
};

#endif // PYTHON_VIEW_WIDGET_HPP
