#ifndef GLYPH_STYLE_STATE_HPP
#define GLYPH_STYLE_STATE_HPP

/**
 * @file GlyphStyleState.hpp
 * @brief QObject wrapper for GlyphStyleData with change signals
 *
 * GlyphStyleState is a concrete QObject that can be composed into plot state
 * classes to provide glyph customization functionality. It wraps the
 * serializable GlyphStyleData and emits signals when properties change.
 *
 * **Pattern**: Mirrors VerticalAxisState — composable, signal-emitting wrapper
 * around a plain serializable struct.
 *
 * **Signal Architecture**:
 * - `styleChanged()` — emitted on any user-driven property change (triggers scene rebuild)
 * - `styleUpdated()` — emitted on programmatic/silent updates (refreshes UI only)
 */

#include "CorePlotting/DataTypes/GlyphStyleData.hpp"

#include <QObject>

/**
 * @brief QObject wrapper for glyph style settings
 *
 * Wraps CorePlotting::GlyphStyleData and emits signals on change.
 * Composable into plot state classes as a member variable.
 */
class GlyphStyleState : public QObject {
    Q_OBJECT

public:
    explicit GlyphStyleState(QObject * parent = nullptr);
    ~GlyphStyleState() override = default;

    // === Accessors ===

    [[nodiscard]] CorePlotting::GlyphType glyphType() const;
    void setGlyphType(CorePlotting::GlyphType type);

    [[nodiscard]] float size() const;
    void setSize(float size);

    [[nodiscard]] std::string const & hexColor() const;
    void setHexColor(std::string const & color);

    [[nodiscard]] float alpha() const;
    void setAlpha(float alpha);

    // === Bulk Operations ===

    /**
     * @brief Set all style properties at once (emits styleChanged)
     */
    void setStyle(CorePlotting::GlyphStyleData const & style);

    /**
     * @brief Set all style properties silently (emits styleUpdated, not styleChanged)
     *
     * Use when loading from serialized state to avoid triggering scene rebuilds.
     */
    void setStyleSilent(CorePlotting::GlyphStyleData const & style);

    // === Data Access ===

    [[nodiscard]] CorePlotting::GlyphStyleData const & data() const { return _data; }
    [[nodiscard]] CorePlotting::GlyphStyleData & data() { return _data; }

signals:
    /**
     * @brief Emitted when any glyph style property changes (user action)
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
    CorePlotting::GlyphStyleData _data;
};

#endif// GLYPH_STYLE_STATE_HPP
