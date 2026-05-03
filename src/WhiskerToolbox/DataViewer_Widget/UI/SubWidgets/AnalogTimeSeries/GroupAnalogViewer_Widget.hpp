/**
 * @file GroupAnalogViewer_Widget.hpp
 * @brief Batch property editor for a group of analog time series in DataViewer.
 */

#ifndef GROUPANALOGVIEWER_WIDGET_HPP
#define GROUPANALOGVIEWER_WIDGET_HPP

#include <QWidget>

#include <string>
#include <vector>

class DataManager;
class OpenGLWidget;

class GroupAnalogViewer_Widget : public QWidget {
    Q_OBJECT
public:
    explicit GroupAnalogViewer_Widget(std::shared_ptr<DataManager> data_manager,
                                      OpenGLWidget * opengl_widget,
                                      QWidget * parent = nullptr);
    ~GroupAnalogViewer_Widget() override;

    void setActiveKeys(std::string const & group_name,
                       std::vector<std::string> const & keys);

signals:
    void colorChanged(std::string const & feature_key, std::string const & hex_color);

private slots:
    void _openColorDialog();
    void _onAlphaChanged(int value);
    void _onScaleChanged(double value);
    void _onThicknessChanged(double value);
    void _onMinMaxDecimationToggled(bool enabled);

private:
    void _applyToAllKeys(std::function<void(std::string const &)> const & fn);

    std::shared_ptr<DataManager> _data_manager;
    OpenGLWidget * _opengl_widget;
    std::string _group_name;
    std::vector<std::string> _active_keys;

    // UI elements (created in code, no .ui file needed)
    class QLabel * _name_label{nullptr};
    class QLabel * _count_label{nullptr};
    class QPushButton * _color_display_button{nullptr};
    class QPushButton * _color_button{nullptr};
    class QSlider * _alpha_slider{nullptr};
    class QLabel * _alpha_value_label{nullptr};
    class QDoubleSpinBox * _scale_spinbox{nullptr};
    class QDoubleSpinBox * _thickness_spinbox{nullptr};
    class QCheckBox * _min_max_decimation_checkbox{nullptr};

    bool _updating{false};
};

#endif// GROUPANALOGVIEWER_WIDGET_HPP
