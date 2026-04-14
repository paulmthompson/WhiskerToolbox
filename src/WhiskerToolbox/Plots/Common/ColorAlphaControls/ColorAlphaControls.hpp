#ifndef COLOR_ALPHA_CONTROLS_HPP
#define COLOR_ALPHA_CONTROLS_HPP

/**
 * @file ColorAlphaControls.hpp
 * @brief Compact widget for editing color and alpha properties
 *
 * ColorAlphaControls provides a minimal widget with:
 * - Color button (opens QColorDialog on click)
 * - Alpha spin box (0.0 – 1.0)
 *
 * Designed to be embedded in properties panels of any widget that
 * needs color + alpha editing without the overhead of a full state
 * object.  Follows the same visual style as GlyphStyleControls and
 * LineStyleControls.
 *
 * **Usage Pattern:**
 * @code
 *   auto * controls = new ColorAlphaControls(parent);
 *   controls->setColor("#FF0000");
 *   controls->setAlpha(0.5f);
 *   connect(controls, &ColorAlphaControls::colorChanged, ...);
 *   connect(controls, &ColorAlphaControls::alphaChanged, ...);
 * @endcode
 */

#include <QWidget>

class QDoubleSpinBox;
class QPushButton;

/**
 * @brief Compact color + alpha editing widget
 *
 * Provides a colored QPushButton that opens QColorDialog and a
 * QDoubleSpinBox for alpha (0.0 – 1.0).
 */
class ColorAlphaControls : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct color/alpha controls
     * @param parent Parent widget
     */
    explicit ColorAlphaControls(QWidget * parent = nullptr);

    ~ColorAlphaControls() override = default;

    /**
     * @brief Set the current color
     * @param hex_color Hex color string (e.g. "#FF0000")
     */
    void setColor(QString const & hex_color);

    /**
     * @brief Set the current alpha
     * @param alpha Alpha value in [0.0, 1.0]
     */
    void setAlpha(float alpha);

    /**
     * @brief Get the current hex color
     */
    [[nodiscard]] QString hexColor() const;

    /**
     * @brief Get the current alpha
     */
    [[nodiscard]] float alpha() const;

    /**
     * @brief Get the color button (for external layout tweaks)
     */
    [[nodiscard]] QPushButton * colorButton() const { return _color_button; }

    /**
     * @brief Get the alpha spin box (for external layout tweaks)
     */
    [[nodiscard]] QDoubleSpinBox * alphaSpinBox() const { return _alpha_spinbox; }

signals:
    /**
     * @brief Emitted when the user picks a new color
     * @param hex_color The new hex color string
     */
    void colorChanged(QString const & hex_color);

    /**
     * @brief Emitted when the user changes the alpha value
     * @param alpha The new alpha in [0.0, 1.0]
     */
    void alphaChanged(float alpha);

private slots:
    void _onColorClicked();
    void _onAlphaChanged(double value);

private:
    QPushButton * _color_button;
    QDoubleSpinBox * _alpha_spinbox;
    QString _hex_color{"#FF0000"};

    void _updateColorButtonStyle();
};

#endif// COLOR_ALPHA_CONTROLS_HPP
