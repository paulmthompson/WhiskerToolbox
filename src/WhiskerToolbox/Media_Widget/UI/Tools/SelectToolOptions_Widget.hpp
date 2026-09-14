#ifndef SELECT_TOOL_OPTIONS_WIDGET_HPP
#define SELECT_TOOL_OPTIONS_WIDGET_HPP

/**
 * @file SelectToolOptions_Widget.hpp
 * @brief Options controls for the Media Viewer Select tool
 */

#include <QWidget>

class MediaWidgetState;
class QComboBox;
class QHBoxLayout;
class QLabel;
class QSpinBox;

/**
 * @brief Horizontal options for the Select tool, including optional key filtering
 */
class SelectToolOptions_Widget : public QWidget {
    Q_OBJECT

public:
    explicit SelectToolOptions_Widget(QWidget * parent = nullptr);

    /**
     * @brief Bind to shared media widget state
     * @param state State object (non-owning)
     */
    void setState(MediaWidgetState * state);

private slots:
    void _onFilterComboChanged(int index);
    void _onPickRadiusChanged(int radius_px);
    void _onEnabledFeaturesChanged();
    void _syncFromState();

private:
    void _buildUi();
    void _rebuildFilterCombo();
    [[nodiscard]] int _indexForCurrentFilter() const;

    MediaWidgetState * _state{nullptr};
    bool _updating_from_state{false};

    QHBoxLayout * _layout{nullptr};
    QLabel * _filter_label{nullptr};
    QComboBox * _filter_combo{nullptr};
    QLabel * _pick_radius_label{nullptr};
    QSpinBox * _pick_radius_spinbox{nullptr};
};

#endif// SELECT_TOOL_OPTIONS_WIDGET_HPP
