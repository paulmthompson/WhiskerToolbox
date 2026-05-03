/**
 * @file GroupEventViewer_Widget.hpp
 * @brief Batch property editor for a group of digital event series in DataViewer.
 */

#ifndef GROUPEVENTVIEWER_WIDGET_HPP
#define GROUPEVENTVIEWER_WIDGET_HPP

#include <QWidget>

#include <functional>
#include <memory>
#include <string>
#include <vector>

class DataManager;
class OpenGLWidget;

/**
 * @brief Properties for all digital event series in a feature-tree group selection
 *
 * Applies display options (color, alpha) from this panel to every series key in
 * the active group. Uses the first key to seed the UI when the selection changes.
 */
class GroupEventViewer_Widget : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Construct a group event properties widget
     *
     * @param data_manager Shared DataManager (reserved for future group-aware queries)
     * @param opengl_widget Non-owning pointer to the plot widget whose state is edited
     * @param parent Optional Qt parent
     */
    explicit GroupEventViewer_Widget(std::shared_ptr<DataManager> data_manager,
                                     OpenGLWidget * opengl_widget,
                                     QWidget * parent = nullptr);
    ~GroupEventViewer_Widget() override;

    /**
     * @brief Bind the widget to a named group and its member series keys
     *
     * @param group_name Display name of the group in the feature tree
     * @param keys DataManager keys for each DigitalEvent series in the group
     */
    void setActiveKeys(std::string const & group_name, std::vector<std::string> const & keys);

signals:
    void colorChanged(std::string const & feature_key, std::string const & hex_color);

private slots:
    void _openColorDialog();
    void _onAlphaChanged(int value);

    /**
     * @brief Assign a distinct color to each series in `_active_keys` using a random hue wheel offset
     */
    void _assignRandomUniqueColors();

private:
    /**
     * @brief Invoke @p fn for each key in `_active_keys`
     * @param fn Callback receiving one series key at a time
     */
    void _applyToAllKeys(std::function<void(std::string const &)> const & fn);

    std::shared_ptr<DataManager> _data_manager;
    OpenGLWidget * _opengl_widget;
    std::string _group_name;
    std::vector<std::string> _active_keys;

    class QLabel * _name_label{nullptr};
    class QLabel * _count_label{nullptr};
    class QPushButton * _color_display_button{nullptr};
    class QPushButton * _color_button{nullptr};
    class QPushButton * _random_unique_colors_button{nullptr};
    class QSlider * _alpha_slider{nullptr};
    class QLabel * _alpha_value_label{nullptr};

    bool _updating{false};
};

#endif// GROUPEVENTVIEWER_WIDGET_HPP
