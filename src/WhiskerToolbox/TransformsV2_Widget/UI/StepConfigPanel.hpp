#ifndef WHISKERTOOLBOX_STEP_CONFIG_PANEL_HPP
#define WHISKERTOOLBOX_STEP_CONFIG_PANEL_HPP

/**
 * @file StepConfigPanel.hpp
 * @brief Configuration panel for a single pipeline step
 *
 * When a step is selected in PipelineStepListWidget, this panel displays
 * the parameter editing UI for that step. It uses AutoParamWidget for
 * auto-generated forms, with ParamWidgetRegistry override support.
 */

#include <QWidget>

#include <memory>
#include <string>

class QLabel;
class QVBoxLayout;
class QScrollArea;
class AutoParamWidget;

struct ParameterSchema;
struct TransformMetadata;

/**
 * @brief Configuration panel for a single transform step's parameters
 *
 * Looks up the parameter schema from ElementRegistry and displays either:
 * - A custom widget (from ParamWidgetRegistry), or
 * - An auto-generated form (from AutoParamWidget)
 *
 * Provides bidirectional JSON sync for the step's parameters.
 */
class StepConfigPanel : public QWidget {
    Q_OBJECT

public:
    explicit StepConfigPanel(QWidget * parent = nullptr);
    ~StepConfigPanel() override;

    /**
     * @brief Show the configuration for a specific transform step
     * @param transform_name The registered transform name
     * @param params_json Existing parameters as JSON to populate the form
     */
    void showStepConfig(std::string const & transform_name,
                        std::string const & params_json);

    /**
     * @brief Clear the panel (no step selected)
     */
    void clearConfig();

    /**
     * @brief Get the current parameters as a JSON string
     * @return JSON string of current parameter values
     */
    [[nodiscard]] std::string currentParamsJson() const;

    /**
     * @brief Get the name of the currently displayed transform
     */
    [[nodiscard]] std::string currentTransformName() const { return _current_transform_name; }

signals:
    /**
     * @brief Emitted when any parameter value changes
     * @param params_json Updated parameters as JSON
     */
    void parametersChanged(std::string const & params_json);

private:
    void setupAutoParamWidget(std::string const & transform_name,
                              std::string const & params_json);
    void setupCustomWidget(std::string const & transform_name,
                           std::string const & params_json);
    void clearCurrentWidget();

    QVBoxLayout * _main_layout = nullptr;
    QLabel * _header_label = nullptr;
    QLabel * _description_label = nullptr;
    QScrollArea * _scroll_area = nullptr;
    QWidget * _scroll_content = nullptr;
    QVBoxLayout * _scroll_layout = nullptr;

    // Current active widget (either auto or custom)
    AutoParamWidget * _auto_param_widget = nullptr;
    QWidget * _custom_widget = nullptr;

    std::string _current_transform_name;
};

#endif// WHISKERTOOLBOX_STEP_CONFIG_PANEL_HPP
