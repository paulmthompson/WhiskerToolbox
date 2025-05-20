#ifndef WHISKER_TOOLBOX_LINE_ADD_SELECTION_WIDGET_HPP
#define WHISKER_TOOLBOX_LINE_ADD_SELECTION_WIDGET_HPP

#include <QWidget>

class QButtonGroup;
class QGroupBox;
class QSlider;
class QSpinBox;

namespace Ui {
class LineAddSelectionWidget;
}

namespace line_widget {

/**
 * @brief Widget for the "Add Points" selection mode
 * 
 * This widget provides UI for adding points to lines, including options for:
 * - Edge snapping
 * - Smoothing methods (simple smooth or polynomial fit)
 * - Edge detection parameters
 */
class LineAddSelectionWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent The parent widget
     */
    explicit LineAddSelectionWidget(QWidget* parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~LineAddSelectionWidget();

    /**
     * @brief Get the current edge snapping enabled state
     */
    bool isEdgeSnappingEnabled() const;

    /**
     * @brief Get the current edge threshold value
     */
    int getEdgeThreshold() const;

    /**
     * @brief Get the current edge search radius value
     */
    int getEdgeSearchRadius() const;

    /**
     * @brief Get the current polynomial order value
     */
    int getPolynomialOrder() const;

    /**
     * @brief Enum defining the available smoothing modes
     */
    enum class SmoothingMode {
        SimpleSmooth = 0,
        PolynomialFit = 1
    };

    /**
     * @brief Get the current smoothing mode
     */
    SmoothingMode getSmoothingMode() const;

signals:
    /**
     * @brief Signal emitted when edge snapping is toggled
     * @param enabled Whether edge snapping is enabled
     */
    void edgeSnappingToggled(bool enabled);
    
    /**
     * @brief Signal emitted when smoothing mode is changed
     * @param mode The selected smoothing mode
     */
    void smoothingModeChanged(int mode);
    
    /**
     * @brief Signal emitted when polynomial order is changed
     * @param order The new polynomial order value
     */
    void polynomialOrderChanged(int order);
    
    /**
     * @brief Signal emitted when edge threshold is changed
     * @param threshold The new threshold value
     */
    void edgeThresholdChanged(int threshold);
    
    /**
     * @brief Signal emitted when edge search radius is changed
     * @param radius The new radius value
     */
    void edgeSearchRadiusChanged(int radius);

private:
    Ui::LineAddSelectionWidget* ui;
    QButtonGroup* _smoothingGroup = nullptr;
    QGroupBox* _edgeParamsGroup = nullptr;
    QSlider* _thresholdSlider = nullptr;
    QSpinBox* _radiusSpinBox = nullptr;
    QSpinBox* _polyOrderSpinBox = nullptr;
    
    SmoothingMode _smoothingMode = SmoothingMode::SimpleSmooth;
    bool _edgeSnappingEnabled = false;
    int _edgeThreshold = 100;
    int _edgeSearchRadius = 20;
    int _polynomialOrder = 3;
    
    void _setupUI();
    void _connectSignals();
};

} // namespace line_widget

#endif // WHISKER_TOOLBOX_LINE_ADD_SELECTION_WIDGET_HPP 