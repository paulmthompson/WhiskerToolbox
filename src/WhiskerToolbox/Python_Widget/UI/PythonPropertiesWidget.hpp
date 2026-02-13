#ifndef PYTHON_PROPERTIES_WIDGET_HPP
#define PYTHON_PROPERTIES_WIDGET_HPP

/**
 * @file PythonPropertiesWidget.hpp
 * @brief Properties panel for the Python Widget
 *
 * PythonPropertiesWidget is the properties/inspector panel for PythonViewWidget.
 * It will contain configuration controls (font size, auto-scroll, script path, etc.).
 *
 * Both PythonViewWidget and PythonPropertiesWidget share the same PythonWidgetState.
 *
 * @see PythonWidgetState for the shared state class
 * @see PythonViewWidget for the main view widget
 */

#include <QWidget>

#include <memory>

class PythonWidgetState;

namespace Ui {
class PythonPropertiesWidget;
}

class PythonPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    explicit PythonPropertiesWidget(std::shared_ptr<PythonWidgetState> state,
                                    QWidget * parent = nullptr);
    ~PythonPropertiesWidget() override;

private:
    std::unique_ptr<Ui::PythonPropertiesWidget> ui;
    std::shared_ptr<PythonWidgetState> _state;
};

#endif // PYTHON_PROPERTIES_WIDGET_HPP
