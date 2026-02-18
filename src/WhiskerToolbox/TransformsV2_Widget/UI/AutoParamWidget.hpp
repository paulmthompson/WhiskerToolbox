#ifndef WHISKERTOOLBOX_AUTO_PARAM_WIDGET_HPP
#define WHISKERTOOLBOX_AUTO_PARAM_WIDGET_HPP

/**
 * @file AutoParamWidget.hpp
 * @brief Generic Qt form generator from ParameterSchema
 *
 * Reads a ParameterSchema (extracted from reflect-cpp parameter structs)
 * and dynamically creates form rows with appropriate Qt widgets for each field.
 *
 * Field Type Mapping:
 *   float / double  → QDoubleSpinBox (with optional min/max from validators)
 *   int             → QSpinBox
 *   bool            → QCheckBox
 *   std::string (with allowed_values) → QComboBox
 *   std::string (free-form) → QLineEdit
 *   std::optional<T> → Checkbox-gated inner widget (unchecked = use default)
 *
 * Stores its current state as a JSON object and exposes:
 *   - setSchema()         — rebuild the form from a new schema
 *   - toJson()            — serialize current values to JSON string
 *   - fromJson()          — populate widgets from a JSON string
 *   - parametersChanged() — signal emitted on any edit
 */

#include <QWidget>

#include <memory>
#include <string>
#include <vector>

namespace WhiskerToolbox::Transforms::V2 {
struct ParameterSchema;
struct ParameterFieldDescriptor;
}// namespace WhiskerToolbox::Transforms::V2

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QGroupBox;
class QLineEdit;
class QSpinBox;
class QVBoxLayout;

/**
 * @brief Generic Qt form widget generated from a ParameterSchema
 *
 * This widget reads a ParameterSchema and dynamically creates a form with
 * appropriate Qt widgets for each field. It provides bidirectional
 * JSON ↔ UI synchronization.
 */
class AutoParamWidget : public QWidget {
    Q_OBJECT

public:
    explicit AutoParamWidget(QWidget * parent = nullptr);
    ~AutoParamWidget() override;

    /**
     * @brief Set the schema and rebuild the form
     * @param schema The parameter schema to generate UI from
     */
    void setSchema(WhiskerToolbox::Transforms::V2::ParameterSchema const & schema);

    /**
     * @brief Serialize current widget values to a JSON string
     * @return JSON string representing current parameter values
     */
    [[nodiscard]] std::string toJson() const;

    /**
     * @brief Populate widget values from a JSON string
     * @param json JSON string to load values from
     * @return true if parsing succeeded (partial updates still applied)
     */
    bool fromJson(std::string const & json);

    /**
     * @brief Clear all fields and remove the schema
     */
    void clear();

signals:
    /**
     * @brief Emitted whenever any parameter value changes
     */
    void parametersChanged();

private:
    // Internal representation of a generated field row
    struct FieldRow {
        std::string field_name;
        std::string type_name;
        bool is_optional = false;

        // The actual value widget (one of these is non-null)
        QDoubleSpinBox * double_spin = nullptr;
        QSpinBox * int_spin = nullptr;
        QCheckBox * bool_check = nullptr;
        QComboBox * combo_box = nullptr;
        QLineEdit * line_edit = nullptr;

        // For optional fields: checkbox that gates the value widget
        QCheckBox * optional_gate = nullptr;
    };

    void buildFieldRow(WhiskerToolbox::Transforms::V2::ParameterFieldDescriptor const & desc,
                       QFormLayout * layout);
    void clearLayout();
    void onFieldChanged();

    QVBoxLayout * _main_layout = nullptr;
    QFormLayout * _form_layout = nullptr;
    QGroupBox * _advanced_group = nullptr;
    QFormLayout * _advanced_layout = nullptr;

    std::vector<FieldRow> _field_rows;
    bool _updating = false;// Guard against recursive signal emission
};

#endif// WHISKERTOOLBOX_AUTO_PARAM_WIDGET_HPP
