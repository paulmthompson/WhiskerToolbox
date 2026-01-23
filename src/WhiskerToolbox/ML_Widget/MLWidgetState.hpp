#ifndef ML_WIDGET_STATE_HPP
#define ML_WIDGET_STATE_HPP

/**
 * @file MLWidgetState.hpp
 * @brief State class for ML_Widget
 * 
 * MLWidgetState manages the serializable state for the ML_Widget,
 * enabling workspace save/restore. The ML_Widget is a properties-zone
 * widget (tool panel) that provides machine learning model training
 * and prediction capabilities.
 * 
 * ## Design Pattern
 * 
 * ML_Widget follows the "tool widget" pattern where the widget itself
 * is placed in Zone::Right (properties zone) without a separate view.
 * This is similar to DataTransformWidget.
 * 
 * ## State Tracked
 * 
 * - Table selection and column configuration
 * - Training interval and model type
 * - Selected outcomes for classification
 * 
 * @see EditorState for base class documentation
 * @see MLWidgetStateData for the serializable data structure
 * @see ML_Widget for the widget implementation
 */

#include "EditorState/EditorState.hpp"
#include "MLWidgetStateData.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

// Forward declarations
class DataManager;

/**
 * @brief State class for ML_Widget
 * 
 * MLWidgetState is a tool-panel EditorState that tracks machine learning
 * configuration. It's designed to be placed in Zone::Right as a persistent
 * tab alongside other tool widgets.
 */
class MLWidgetState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new MLWidgetState
     * @param parent Parent QObject (typically nullptr, managed by EditorRegistry)
     */
    explicit MLWidgetState(QObject * parent = nullptr);

    ~MLWidgetState() override = default;

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "MLWidget"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("MLWidget"); }

    /**
     * @brief Get the display name for UI
     * @return User-visible name (default: "Machine Learning")
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

    // === Table Selection ===

    [[nodiscard]] QString selectedTableId() const;
    void setSelectedTableId(QString const & table_id);

    [[nodiscard]] std::vector<std::string> selectedFeatureColumns() const { return _data.selected_feature_columns; }
    void setSelectedFeatureColumns(std::vector<std::string> const & columns);

    [[nodiscard]] std::vector<std::string> selectedMaskColumns() const { return _data.selected_mask_columns; }
    void setSelectedMaskColumns(std::vector<std::string> const & columns);

    [[nodiscard]] QString selectedLabelColumn() const;
    void setSelectedLabelColumn(QString const & column);

    // === Training Configuration ===

    [[nodiscard]] QString trainingIntervalKey() const;
    void setTrainingIntervalKey(QString const & key);

    [[nodiscard]] QString selectedModelType() const;
    void setSelectedModelType(QString const & model_type);

    // === Outcomes ===

    [[nodiscard]] std::vector<std::string> selectedOutcomes() const { return _data.selected_outcomes; }
    void setSelectedOutcomes(std::vector<std::string> const & outcomes);
    void addSelectedOutcome(std::string const & outcome);
    void removeSelectedOutcome(std::string const & outcome);

signals:
    // === Table Selection Signals ===
    void selectedTableIdChanged(QString const & table_id);
    void selectedFeatureColumnsChanged(std::vector<std::string> const & columns);
    void selectedMaskColumnsChanged(std::vector<std::string> const & columns);
    void selectedLabelColumnChanged(QString const & column);

    // === Training Configuration Signals ===
    void trainingIntervalKeyChanged(QString const & key);
    void selectedModelTypeChanged(QString const & model_type);

    // === Outcome Signals ===
    void selectedOutcomesChanged(std::vector<std::string> const & outcomes);

private:
    MLWidgetStateData _data;
};

#endif // ML_WIDGET_STATE_HPP
