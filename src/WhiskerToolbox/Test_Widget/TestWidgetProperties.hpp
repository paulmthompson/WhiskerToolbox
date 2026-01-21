#ifndef TEST_WIDGET_PROPERTIES_HPP
#define TEST_WIDGET_PROPERTIES_HPP

/**
 * @file TestWidgetProperties.hpp
 * @brief Properties/controls component for TestWidget (View/Properties split proof-of-concept)
 * 
 * TestWidgetProperties provides the controls for modifying TestWidgetState:
 * - Checkboxes for boolean toggles
 * - Color picker for highlight color
 * - Slider for zoom level
 * - Spinbox for grid spacing
 * - Line edit for label text
 * 
 * All controls modify the shared TestWidgetState. When state changes,
 * TestWidgetView automatically updates via Qt signal connections.
 * 
 * ## View/Properties Split Pattern
 * 
 * This class demonstrates the "Properties" side of the split:
 * - Receives state via shared_ptr (same instance as TestWidgetView)
 * - Provides UI controls that modify state via setters
 * - Connects to state signals to keep controls synchronized
 * - Does NOT display the main visualization (that's TestWidgetView)
 * 
 * ## PropertiesHost Integration
 * 
 * TestWidgetProperties is designed to be shown in the PropertiesHost
 * when the corresponding TestWidgetView is active:
 * 
 * ```cpp
 * // Register with PropertiesHost
 * properties_host->registerEditorPropertiesFactory(
 *     "TestWidget",
 *     [](std::shared_ptr<EditorState> state) {
 *         auto test_state = std::dynamic_pointer_cast<TestWidgetState>(state);
 *         return new TestWidgetProperties(test_state);
 *     }
 * );
 * ```
 * 
 * @see TestWidgetState for the shared state
 * @see TestWidgetView for the visualization component
 */

#include <QWidget>
#include <QCheckBox>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QLineEdit>
#include <QLabel>

#include <memory>

class TestWidgetState;

/**
 * @brief Properties/controls component for TestWidget
 * 
 * Provides UI controls for all TestWidgetState properties.
 * Changes are written to state immediately, causing connected
 * TestWidgetView instances to update.
 */
class TestWidgetProperties : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a new TestWidgetProperties
     * @param state Shared pointer to the state (shared with TestWidgetView)
     * @param parent Parent widget
     */
    explicit TestWidgetProperties(std::shared_ptr<TestWidgetState> state,
                                  QWidget * parent = nullptr);

    ~TestWidgetProperties() override = default;

private slots:
    // === UI Event Handlers ===
    void onShowGridToggled(bool checked);
    void onShowCrosshairToggled(bool checked);
    void onEnableAnimationToggled(bool checked);
    void onColorButtonClicked();
    void onZoomSliderChanged(int value);
    void onGridSpacingChanged(int value);
    void onLabelTextChanged(QString const & text);

    // === State Change Handlers (for external updates) ===
    void onStateShowGridChanged(bool show);
    void onStateShowCrosshairChanged(bool show);
    void onStateEnableAnimationChanged(bool enable);
    void onStateHighlightColorChanged(QColor const & color);
    void onStateZoomLevelChanged(double zoom);
    void onStateGridSpacingChanged(int spacing);
    void onStateLabelTextChanged(QString const & text);

private:
    void setupUi();
    void connectUiSignals();
    void connectStateSignals();
    void updateColorButtonStyle();

    std::shared_ptr<TestWidgetState> _state;

    // === UI Controls ===
    QCheckBox * _show_grid_checkbox = nullptr;
    QCheckBox * _show_crosshair_checkbox = nullptr;
    QCheckBox * _enable_animation_checkbox = nullptr;
    QPushButton * _color_button = nullptr;
    QSlider * _zoom_slider = nullptr;
    QLabel * _zoom_label = nullptr;
    QSpinBox * _grid_spacing_spinbox = nullptr;
    QLineEdit * _label_text_edit = nullptr;

    // Block signals flag to prevent feedback loops
    bool _updating_from_state = false;
};

#endif // TEST_WIDGET_PROPERTIES_HPP
