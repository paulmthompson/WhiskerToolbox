#ifndef GLYPH_STYLE_CONTROLS_HPP
#define GLYPH_STYLE_CONTROLS_HPP

/**
 * @file GlyphStyleControls.hpp
 * @brief Widget for editing glyph style properties (shape, size, color, alpha)
 *
 * GlyphStyleControls provides a compact widget with:
 * - Glyph type combo box (Circle, Square, Tick, Cross, Diamond, Triangle)
 * - Size spin box
 * - Color button (opens QColorDialog)
 * - Alpha spin box
 *
 * It is linked to a GlyphStyleState and handles anti-recursion.
 * Designed to be embedded in properties panels of any widget that
 * renders discrete markers.
 *
 * **Usage Pattern** (mirrors VerticalAxisRangeControls):
 * @code
 *   auto * controls = new GlyphStyleControls(my_state->glyphStyleState(), parent);
 *   properties_layout->addWidget(controls);
 * @endcode
 */

#include <QWidget>

class GlyphStyleState;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;

/**
 * @brief Widget for editing glyph style properties
 *
 * Provides combo box for shape, spin box for size, color button,
 * and alpha slider. Automatically synchronizes with GlyphStyleState.
 */
class GlyphStyleControls : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct glyph style controls
     * @param state GlyphStyleState to bind to (must outlive this widget)
     * @param parent Parent widget
     */
    explicit GlyphStyleControls(
        GlyphStyleState * state,
        QWidget * parent = nullptr);

    ~GlyphStyleControls() override = default;

    /**
     * @brief Get the glyph type combo box
     */
    [[nodiscard]] QComboBox * glyphTypeCombo() const { return _type_combo; }

    /**
     * @brief Get the size spin box
     */
    [[nodiscard]] QDoubleSpinBox * sizeSpinBox() const { return _size_spinbox; }

    /**
     * @brief Get the color button
     */
    [[nodiscard]] QPushButton * colorButton() const { return _color_button; }

    /**
     * @brief Get the alpha spin box
     */
    [[nodiscard]] QDoubleSpinBox * alphaSpinBox() const { return _alpha_spinbox; }

    /**
     * @brief Rebind controls to a different GlyphStyleState
     *
     * Disconnects from the current state (if any), connects to the new state,
     * and refreshes the UI. Pass nullptr to disable the controls.
     *
     * @param state New state to bind to (must outlive this widget while bound)
     */
    void setGlyphStyleState(GlyphStyleState * state);

private slots:
    void onGlyphTypeChanged(int index);
    void onSizeChanged(double value);
    void onColorClicked();
    void onAlphaChanged(double value);

    void onStateStyleChanged();
    void onStateStyleUpdated();

private:
    GlyphStyleState * _state;

    QComboBox * _type_combo;
    QDoubleSpinBox * _size_spinbox;
    QPushButton * _color_button;
    QDoubleSpinBox * _alpha_spinbox;

    bool _updating_ui = false;

    /**
     * @brief Update all UI controls from current state
     */
    void updateFromState();

    /**
     * @brief Update the color button's background to match the current color
     */
    void updateColorButtonStyle();
};

#endif// GLYPH_STYLE_CONTROLS_HPP
