#ifndef TERMINAL_WIDGET_STATE_HPP
#define TERMINAL_WIDGET_STATE_HPP

/**
 * @file TerminalWidgetState.hpp
 * @brief State class for TerminalWidget
 * 
 * TerminalWidgetState manages the serializable state for the TerminalWidget,
 * enabling workspace save/restore and user preference persistence.
 * 
 * ## Design
 * 
 * TerminalWidget is a utility widget that captures stdout/stderr output.
 * Unlike editor widgets with view/properties split, Terminal is a single
 * widget that serves as its own view (no separate properties panel).
 * 
 * ## Usage
 * 
 * ```cpp
 * auto state = std::make_shared<TerminalWidgetState>();
 * 
 * // Configure display preferences
 * state->setAutoScroll(true);
 * state->setShowTimestamps(true);
 * state->setFontSize(12);
 * 
 * // Listen for changes
 * connect(state.get(), &TerminalWidgetState::autoScrollChanged,
 *         this, &MyWidget::onAutoScrollChanged);
 * 
 * // Serialization
 * std::string json = state->toJson();
 * state->fromJson(json);
 * ```
 * 
 * @see EditorState for base class documentation
 * @see TerminalWidgetStateData for the data structure
 * @see TerminalWidget for the view implementation
 */

#include "EditorState/EditorState.hpp"
#include "TerminalWidgetStateData.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <string>

/**
 * @brief State class for TerminalWidget
 * 
 * Provides typed accessors and Qt signals for terminal preferences:
 * - Display settings (auto-scroll, timestamps, word wrap)
 * - Buffer settings (max lines)
 * - Visual settings (font size, font family)
 * - Color settings (background, text, error, system colors)
 */
class TerminalWidgetState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new TerminalWidgetState
     * @param parent Parent QObject (typically nullptr, managed by EditorRegistry)
     */
    explicit TerminalWidgetState(QObject * parent = nullptr);

    ~TerminalWidgetState() override = default;

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "TerminalWidget"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("TerminalWidget"); }

    /**
     * @brief Get the display name for UI
     * @return User-visible name (default: "Terminal")
     */
    [[nodiscard]] QString getDisplayName() const override;

    /**
     * @brief Set the display name
     * @param name New display name
     */
    void setDisplayName(QString const & name) override;

    // === Serialization ===

    /**
     * @brief Serialize state to JSON
     * @return JSON string representation
     */
    [[nodiscard]] std::string toJson() const override;

    /**
     * @brief Restore state from JSON
     * @param json JSON string to parse
     * @return true if parsing succeeded
     */
    bool fromJson(std::string const & json) override;

    // === Display Preferences ===

    [[nodiscard]] bool autoScroll() const { return _data.auto_scroll; }
    void setAutoScroll(bool enabled);

    [[nodiscard]] bool showTimestamps() const { return _data.show_timestamps; }
    void setShowTimestamps(bool show);

    [[nodiscard]] bool wordWrap() const { return _data.word_wrap; }
    void setWordWrap(bool enabled);

    // === Buffer Settings ===

    [[nodiscard]] int maxLines() const { return _data.max_lines; }
    void setMaxLines(int lines);

    // === Visual Settings ===

    [[nodiscard]] int fontSize() const { return _data.font_size; }
    void setFontSize(int size);

    [[nodiscard]] QString fontFamily() const { return QString::fromStdString(_data.font_family); }
    void setFontFamily(QString const & family);

    // === Color Settings ===

    [[nodiscard]] QString backgroundColor() const { return QString::fromStdString(_data.background_color); }
    void setBackgroundColor(QString const & color);

    [[nodiscard]] QString textColor() const { return QString::fromStdString(_data.text_color); }
    void setTextColor(QString const & color);

    [[nodiscard]] QString errorColor() const { return QString::fromStdString(_data.error_color); }
    void setErrorColor(QString const & color);

    [[nodiscard]] QString systemColor() const { return QString::fromStdString(_data.system_color); }
    void setSystemColor(QString const & color);

signals:
    // === Display Preferences ===
    void autoScrollChanged(bool enabled);
    void showTimestampsChanged(bool show);
    void wordWrapChanged(bool enabled);

    // === Buffer Settings ===
    void maxLinesChanged(int lines);

    // === Visual Settings ===
    void fontSizeChanged(int size);
    void fontFamilyChanged(QString const & family);

    // === Color Settings ===
    void backgroundColorChanged(QString const & color);
    void textColorChanged(QString const & color);
    void errorColorChanged(QString const & color);
    void systemColorChanged(QString const & color);

private:
    TerminalWidgetStateData _data;
};

#endif // TERMINAL_WIDGET_STATE_HPP
