/// @file SlotBindingWidget.hpp
/// @brief Composite widget combining a DataManager-aware data source combo
///        with an AutoParamWidget for remaining configuration fields.
///
/// This widget implements the "composite widget pattern" (Phase 0.2 Option C)
/// from the DeepLearning_Widget refactoring roadmap. It keeps AutoParamWidget
/// generic while giving each slot panel full control over data-source selection.
///
/// Layout:
/// ┌───────────────────────────────────────┐
/// │  Label: "Data Source"                 │
/// │  [QComboBox — DataManager keys ▾]     │
/// │                                       │
/// │  AutoParamWidget                      │
/// │  ┌─────────────────────────────────┐  │
/// │  │  field_1:  [widget]             │  │
/// │  │  field_2:  [widget]             │  │
/// │  │  ...                            │  │
/// │  └─────────────────────────────────┘  │
/// └───────────────────────────────────────┘

#ifndef SLOT_BINDING_WIDGET_HPP
#define SLOT_BINDING_WIDGET_HPP

#include "AutoParamWidget/AutoParamWidget.hpp"
#include "DataTypeEnum/DM_DataType.hpp"

#include <QWidget>

#include <memory>
#include <string>
#include <vector>

class QComboBox;
class QLabel;

namespace WhiskerToolbox::Transforms::V2 {
struct ParameterSchema;
}

namespace dl::widget {

class DataSourceComboHelper;

/**
 * @brief Composite widget: data source combo + AutoParamWidget for config.
 *
 * Wraps a QComboBox populated by DataSourceComboHelper with an
 * AutoParamWidget driven by a ParameterSchema. Emits a unified signal
 * whenever either the data source selection or any parameter changes.
 *
 * Typical usage:
 * @code
 * auto * w = new SlotBindingWidget(comboHelper, this);
 * w->setDataSourceLabel("Input Source");
 * w->setDataSourceTypes({"PointData"});
 * w->setParameterSchema(mySchema);
 * connect(w, &SlotBindingWidget::bindingChanged, this, &MyWidget::onBindingChanged);
 * @endcode
 *
 * @note Not thread-safe — must only be used from the GUI thread.
 */
class SlotBindingWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a SlotBindingWidget.
     * @param combo_helper  Shared helper for populating the data source combo.
     *                      The helper's lifetime must exceed this widget's.
     *                      Pass nullptr to hide the data source combo.
     * @param parent  Optional parent widget.
     */
    explicit SlotBindingWidget(
            DataSourceComboHelper * combo_helper = nullptr,
            QWidget * parent = nullptr);

    ~SlotBindingWidget() override;

    // ── Data Source Combo ──────────────────────────────────────────────────

    /**
     * @brief Set the label shown above the data source combo.
     * @param label  Text to display (e.g. "Input Source", "Target Key").
     */
    void setDataSourceLabel(QString const & label);

    /**
     * @brief Set the type filter for the data source combo.
     *
     * Calls DataSourceComboHelper::track() with the given types.
     * Immediately populates the combo.
     *
     * @param types  Data types to include. An empty vector means all keys.
     */
    void setDataSourceTypes(std::vector<DM_DataType> const & types);

    /**
     * @brief Get the currently selected data source key.
     * @return The selected key, or empty string if "(None)" is selected.
     */
    [[nodiscard]] std::string selectedDataSource() const;

    /**
     * @brief Programmatically set the data source selection.
     * @param key  The DataManager key to select. If not found, selects "(None)".
     */
    void setSelectedDataSource(std::string const & key);

    /**
     * @brief Show or hide the data source combo section.
     * @param visible  true to show, false to hide.
     */
    void setDataSourceVisible(bool visible);

    /**
     * @brief Direct access to the data source combo (for advanced wiring).
     */
    [[nodiscard]] QComboBox * dataSourceCombo() const;

    // ── AutoParamWidget ───────────────────────────────────────────────────

    /**
     * @brief Set the parameter schema and rebuild the auto-generated form.
     * @param schema  The schema describing the config parameters.
     */
    void setParameterSchema(
            WhiskerToolbox::Transforms::V2::ParameterSchema const & schema);

    /**
     * @brief Get the current parameter values as JSON.
     * @return JSON string from the AutoParamWidget.
     */
    [[nodiscard]] std::string parametersJson() const;

    /**
     * @brief Set parameter values from a JSON string.
     * @param json  JSON to load into the AutoParamWidget.
     * @return true if parsing succeeded.
     */
    bool setParametersJson(std::string const & json);

    /**
     * @brief Clear the parameter form (removes all parameter fields).
     */
    void clearParameters();

    /**
     * @brief Direct access to the AutoParamWidget (for advanced wiring).
     */
    [[nodiscard]] AutoParamWidget * paramWidget() const;

    /**
     * @brief Install a post-edit hook on the contained AutoParamWidget.
     * @param hook  Callback invoked after each parameter edit.
     */
    void setPostEditHook(AutoParamWidget::PostEditHook hook);

signals:
    /**
     * @brief Emitted when the data source combo selection changes.
     * @param key  The newly selected DataManager key (empty if "(None)").
     */
    void dataSourceChanged(std::string const & key);

    /**
     * @brief Emitted when any auto-generated parameter value changes.
     */
    void parametersChanged();

    /**
     * @brief Unified signal: emitted when either the data source or any
     *        parameter changes.
     *
     * Convenience signal for callers that want a single change notification.
     */
    void bindingChanged();

private:
    DataSourceComboHelper * _combo_helper = nullptr;

    QLabel * _source_label = nullptr;
    QComboBox * _source_combo = nullptr;
    AutoParamWidget * _param_widget = nullptr;
};

}// namespace dl::widget

#endif// SLOT_BINDING_WIDGET_HPP
