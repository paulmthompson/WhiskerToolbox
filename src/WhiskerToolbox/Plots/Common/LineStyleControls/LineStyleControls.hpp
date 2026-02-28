#ifndef LINE_STYLE_CONTROLS_HPP
#define LINE_STYLE_CONTROLS_HPP

/**
 * @file LineStyleControls.hpp
 * @brief Widget for editing line style properties (color, thickness, alpha)
 *
 * LineStyleControls provides a compact widget with:
 * - Color button (opens QColorDialog)
 * - Thickness spin box
 * - Alpha spin box
 *
 * It is linked to a LineStyleState and handles anti-recursion.
 * Designed to be embedded in properties panels of any widget that
 * renders continuous lines or curves.
 *
 * **Usage Pattern** (mirrors GlyphStyleControls):
 * @code
 *   auto * controls = new LineStyleControls(my_state->lineStyleState(), parent);
 *   properties_layout->addWidget(controls);
 * @endcode
 */

#include <QWidget>

class LineStyleState;
class QDoubleSpinBox;
class QPushButton;

/**
 * @brief Widget for editing line style properties
 *
 * Provides color button, thickness spin box, and alpha spin box.
 * Automatically synchronizes with LineStyleState.
 */
class LineStyleControls : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct line style controls
     * @param state LineStyleState to bind to (must outlive this widget)
     * @param parent Parent widget
     */
    explicit LineStyleControls(
        LineStyleState * state,
        QWidget * parent = nullptr);

    ~LineStyleControls() override = default;

    /**
     * @brief Get the color button
     */
    [[nodiscard]] QPushButton * colorButton() const { return _color_button; }

    /**
     * @brief Get the thickness spin box
     */
    [[nodiscard]] QDoubleSpinBox * thicknessSpinBox() const { return _thickness_spinbox; }

    /**
     * @brief Get the alpha spin box
     */
    [[nodiscard]] QDoubleSpinBox * alphaSpinBox() const { return _alpha_spinbox; }

    /**
     * @brief Rebind controls to a different LineStyleState
     *
     * Disconnects from the current state (if any), connects to the new state,
     * and refreshes the UI. Pass nullptr to disable the controls.
     *
     * @param state New state to bind to (must outlive this widget while bound)
     */
    void setLineStyleState(LineStyleState * state);

private slots:
    void onColorClicked();
    void onThicknessChanged(double value);
    void onAlphaChanged(double value);

    void onStateStyleChanged();
    void onStateStyleUpdated();

private:
    LineStyleState * _state;

    QPushButton * _color_button;
    QDoubleSpinBox * _thickness_spinbox;
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

#endif // LINE_STYLE_CONTROLS_HPP
