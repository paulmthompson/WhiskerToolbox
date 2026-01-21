#ifndef TEST_WIDGET_STATE_HPP
#define TEST_WIDGET_STATE_HPP

/**
 * @file TestWidgetState.hpp
 * @brief State class for TestWidget (View/Properties split proof-of-concept)
 * 
 * TestWidgetState manages the serializable state for the TestWidget,
 * demonstrating the View/Properties split pattern where:
 * - TestWidgetView displays visualization based on state
 * - TestWidgetProperties provides controls that modify state
 * - Both components share the same TestWidgetState instance
 * 
 * ## Design Pattern
 * 
 * ```
 *                 WorkspaceManager
 *                       │
 *                       │ shared_ptr
 *                       ▼
 *              ┌─────────────────┐
 *              │ TestWidgetState │
 *              └─────────────────┘
 *                ▲             ▲
 *    shared_ptr  │             │  shared_ptr
 *                │             │
 *     ┌──────────┴──┐    ┌─────┴──────────────┐
 *     │TestWidgetView│   │TestWidgetProperties│
 *     └─────────────┘    └────────────────────┘
 * ```
 * 
 * ## Usage
 * 
 * ```cpp
 * // Create state with DataManager
 * auto dm = std::make_shared<DataManager>();
 * auto state = std::make_shared<TestWidgetState>(dm);
 * 
 * // Create view and properties components
 * auto view = new TestWidgetView(state, parent);
 * auto props = new TestWidgetProperties(state, parent);
 * 
 * // Both components now share the same state
 * // Changes in properties automatically update view via signals
 * ```
 * 
 * @see EditorState for base class documentation
 * @see TestWidgetStateData for the data structure
 * @see TestWidgetView for the visualization component
 * @see TestWidgetProperties for the controls component
 */

#include "EditorState/EditorState.hpp"
#include "TestWidgetStateData.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <QColor>

#include <memory>
#include <string>

// Forward declaration
class DataManager;

/**
 * @brief State class for TestWidget proof-of-concept
 * 
 * Demonstrates the View/Properties split pattern with:
 * - Boolean properties (show_grid, show_crosshair, enable_animation)
 * - Color property (highlight_color)
 * - Numeric properties (zoom_level, grid_spacing)
 * - Text property (label_text)
 * 
 * Each property has:
 * - A getter returning the current value
 * - A setter that emits a signal when the value changes
 * - A Qt signal for change notification
 */
class TestWidgetState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new TestWidgetState
     * @param data_manager Shared pointer to DataManager (standard pattern)
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager)
     */
    explicit TestWidgetState(std::shared_ptr<DataManager> data_manager,
                             QObject * parent = nullptr);

    ~TestWidgetState() override = default;

    // === DataManager Access ===

    /**
     * @brief Get the DataManager
     * @return Shared pointer to DataManager
     */
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "TestWidget"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("TestWidget"); }

    /**
     * @brief Get the display name for UI
     * @return User-visible name (default: "Test Widget")
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

    // === Feature Toggles ===

    [[nodiscard]] bool showGrid() const { return _data.show_grid; }
    void setShowGrid(bool show);

    [[nodiscard]] bool showCrosshair() const { return _data.show_crosshair; }
    void setShowCrosshair(bool show);

    [[nodiscard]] bool enableAnimation() const { return _data.enable_animation; }
    void setEnableAnimation(bool enable);

    // === Color ===

    [[nodiscard]] QColor highlightColor() const;
    [[nodiscard]] QString highlightColorHex() const;
    void setHighlightColor(QColor const & color);
    void setHighlightColor(QString const & hexColor);

    // === Numeric Values ===

    [[nodiscard]] double zoomLevel() const { return _data.zoom_level; }
    void setZoomLevel(double zoom);

    [[nodiscard]] int gridSpacing() const { return _data.grid_spacing; }
    void setGridSpacing(int spacing);

    // === Text ===

    [[nodiscard]] QString labelText() const;
    void setLabelText(QString const & text);

signals:
    // === Feature Toggle Signals ===
    void showGridChanged(bool show);
    void showCrosshairChanged(bool show);
    void enableAnimationChanged(bool enable);

    // === Color Signals ===
    void highlightColorChanged(QColor const & color);

    // === Numeric Signals ===
    void zoomLevelChanged(double zoom);
    void gridSpacingChanged(int spacing);

    // === Text Signals ===
    void labelTextChanged(QString const & text);

private:
    std::shared_ptr<DataManager> _data_manager;
    TestWidgetStateData _data;
};

#endif // TEST_WIDGET_STATE_HPP
