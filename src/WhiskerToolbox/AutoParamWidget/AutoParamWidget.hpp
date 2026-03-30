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

#include <functional>
#include <memory>
#include <string>
#include <vector>

struct ParameterSchema;
struct ParameterFieldDescriptor;

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QGroupBox;
class QLineEdit;
class QSpinBox;
class QStackedWidget;
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
    void setSchema(ParameterSchema const & schema);

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

    /**
     * @brief Callback type for post-edit hooks
     *
     * Receives the current JSON from the widget after a user edit.
     * Should return the (possibly modified) JSON. If the returned JSON
     * differs from the input, the widget re-populates itself with the
     * new values (without re-triggering the hook).
     *
     * Use cases:
     *   - Bidirectional derived-field calculation (e.g., alpha/beta ↔ min/max)
     *   - Clamping or snapping values (e.g., forcing odd kernel sizes)
     */
    using PostEditHook = std::function<std::string(std::string const & json)>;

    /**
     * @brief Install a post-edit hook that can transform parameter values after each edit
     * @param hook Callback invoked after each edit; pass nullptr / empty function to remove
     */
    void setPostEditHook(PostEditHook hook);

    /**
     * @brief Dynamically update the combo items for a string/enum field.
     *
     * Finds the FieldRow(s) matching field_name. If the field has a combo_box,
     * repopulates it with values while preserving the current selection.
     * If include_none_sentinel was set, "(None)" is prepended.
     * If the field is a QLineEdit (non-dynamic), this is a no-op.
     *
     * Does not emit parametersChanged unless the selection actually changes.
     */
    void updateAllowedValues(std::string const & field_name,
                             std::vector<std::string> const & values);

    /**
     * @brief Show or hide variant alternatives by tag name.
     *
     * Finds the variant FieldRow matching field_name. Hides alternatives
     * whose tags are not in allowed_tags. If the currently selected
     * alternative is hidden, switches to the first visible one.
     *
     * Does not emit parametersChanged unless the active alternative changes.
     */
    void updateVariantAlternatives(std::string const & field_name,
                                   std::vector<std::string> const & allowed_tags);

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

        // For dynamic combo fields
        bool include_none_sentinel = false;///< Whether to prepend "(None)" to combo items

        // For variant (TaggedUnion) fields
        bool is_variant = false;
        std::string variant_discriminator;
        QComboBox * variant_combo = nullptr;
        QStackedWidget * variant_stack = nullptr;
        std::vector<std::vector<FieldRow>> variant_sub_rows;///< One vector per alternative
        std::vector<std::string> variant_all_tags;          ///< All variant tags from schema
    };

    void buildFieldRow(ParameterFieldDescriptor const & desc,
                       QFormLayout * layout);
    void buildVariantRow(ParameterFieldDescriptor const & desc,
                         QFormLayout * layout);
    static std::string variantSubRowsToJson(FieldRow const & row);
    static void variantSubRowsFromJson(FieldRow & row, std::string const & json_obj_str);
    void clearLayout();
    void onFieldChanged();

    QVBoxLayout * _main_layout = nullptr;
    QFormLayout * _form_layout = nullptr;
    QGroupBox * _advanced_group = nullptr;
    QFormLayout * _advanced_layout = nullptr;

    std::vector<FieldRow> _field_rows;
    bool _updating = false;      // Guard against recursive signal emission
    PostEditHook _post_edit_hook;// Optional post-edit transformation hook
};

#endif// WHISKERTOOLBOX_AUTO_PARAM_WIDGET_HPP
