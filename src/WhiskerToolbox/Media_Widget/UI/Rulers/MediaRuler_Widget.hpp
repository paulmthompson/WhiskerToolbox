#ifndef MEDIA_RULER_WIDGET_HPP
#define MEDIA_RULER_WIDGET_HPP

/**
 * @file MediaRuler_Widget.hpp
 * @brief Properties panel controls for Media Viewer pixel rulers
 */

#include "Core/MediaWidgetStateData.hpp"

#include <QWidget>

class MediaWidgetState;
class QCheckBox;
class QComboBox;
class QPushButton;
class QSpinBox;

/**
 * @brief Collapsible-section content for ruler display preferences
 */
class MediaRuler_Widget : public QWidget {
    Q_OBJECT

public:
    explicit MediaRuler_Widget(QWidget * parent = nullptr);

    /**
     * @brief Bind to shared media widget state
     * @param state State object (non-owning)
     */
    void setState(MediaWidgetState * state);

private slots:
    void _onPrefsChanged();
    void _onNegativeColorClicked();
    void _syncFromState();

private:
    void _buildUi();
    void _updateModeControls();
    [[nodiscard]] RulerPrefs _readPrefsFromUi() const;

    MediaWidgetState * _state{nullptr};
    bool _updating_from_state{false};

    QCheckBox * _enabled_checkbox{nullptr};
    QComboBox * _tick_mode_combo{nullptr};
    QSpinBox * _fixed_interval_spin{nullptr};
    QSpinBox * _target_tick_count_spin{nullptr};
    QCheckBox * _minor_ticks_checkbox{nullptr};
    QPushButton * _negative_color_button{nullptr};
};

#endif// MEDIA_RULER_WIDGET_HPP
