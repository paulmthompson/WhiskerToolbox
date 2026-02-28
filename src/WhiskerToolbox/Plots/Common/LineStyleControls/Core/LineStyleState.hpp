#ifndef LINE_STYLE_STATE_HPP
#define LINE_STYLE_STATE_HPP

/**
 * @file LineStyleState.hpp
 * @brief QObject wrapper for LineStyleData with change signals
 *
 * LineStyleState is a concrete QObject that can be composed into plot state
 * classes to provide line style customization functionality. It wraps the
 * serializable LineStyleData and emits signals when properties change.
 *
 * **Pattern**: Mirrors GlyphStyleState — composable, signal-emitting wrapper
 * around a plain serializable struct.
 *
 * **Signal Architecture**:
 * - `styleChanged()` — emitted on any user-driven property change (triggers scene rebuild)
 * - `styleUpdated()` — emitted on programmatic/silent updates (refreshes UI only)
 */

#include "CorePlotting/DataTypes/LineStyleData.hpp"

#include <QObject>

/**
 * @brief QObject wrapper for line style settings
 *
 * Wraps CorePlotting::LineStyleData and emits signals on change.
 * Composable into plot state classes as a member variable.
 */
class LineStyleState : public QObject {
    Q_OBJECT

public:
    explicit LineStyleState(QObject * parent = nullptr);
    ~LineStyleState() override = default;

    // === Accessors ===

    [[nodiscard]] std::string const & hexColor() const;
    void setHexColor(std::string const & color);

    [[nodiscard]] float thickness() const;
    void setThickness(float thickness);

    [[nodiscard]] float alpha() const;
    void setAlpha(float alpha);

    // === Bulk Operations ===

    /**
     * @brief Set all style properties at once (emits styleChanged)
     */
    void setStyle(CorePlotting::LineStyleData const & style);

    /**
     * @brief Set all style properties silently (emits styleUpdated, not styleChanged)
     *
     * Use when loading from serialized state to avoid triggering scene rebuilds.
     */
    void setStyleSilent(CorePlotting::LineStyleData const & style);

    // === Data Access ===

    [[nodiscard]] CorePlotting::LineStyleData const & data() const { return _data; }
    [[nodiscard]] CorePlotting::LineStyleData & data() { return _data; }

signals:
    /**
     * @brief Emitted when any line style property changes (user action)
     *
     * Connect to this to trigger scene rebuilds.
     */
    void styleChanged();

    /**
     * @brief Emitted when style is updated programmatically (e.g., deserialization)
     *
     * Connect to this for UI refresh without triggering scene rebuilds.
     */
    void styleUpdated();

private:
    CorePlotting::LineStyleData _data;
};

#endif // LINE_STYLE_STATE_HPP
