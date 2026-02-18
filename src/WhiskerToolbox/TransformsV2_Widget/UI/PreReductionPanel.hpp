#ifndef WHISKERTOOLBOX_PRE_REDUCTION_PANEL_HPP
#define WHISKERTOOLBOX_PRE_REDUCTION_PANEL_HPP

/**
 * @file PreReductionPanel.hpp
 * @brief Panel for configuring pre-reduction steps in a pipeline
 *
 * Pre-reductions are computed before the main pipeline and store results
 * in a value store that pipeline steps can reference via param bindings.
 * Example: ZScore needs mean and stddev pre-computed over the full range.
 */

#include <QWidget>

#include <string>
#include <typeindex>
#include <vector>

class QGroupBox;
class QListWidget;
class QPushButton;
class QVBoxLayout;

namespace WhiskerToolbox::Transforms::V2::Examples {
struct PreReductionStepDescriptor;
} // namespace WhiskerToolbox::Transforms::V2::Examples

/**
 * @brief Represents a single pre-reduction step in the UI
 */
struct PreReductionEntry {
    std::string reduction_name;   ///< Name from RangeReductionRegistry
    std::string output_key;       ///< Key used for param binding
    std::string parameters_json;  ///< Optional parameters as JSON
};

/**
 * @brief Collapsible panel for managing pre-reduction steps
 *
 * Pre-reductions are computed over the entire input data range before
 * the main pipeline executes. The results are stored in a value store
 * and can be referenced via param_bindings in pipeline steps.
 *
 * Example: ZScoreNormalizeV2 requires "mean" and "stddev" pre-reductions.
 */
class PreReductionPanel : public QWidget {
    Q_OBJECT

public:
    explicit PreReductionPanel(QWidget * parent = nullptr);
    ~PreReductionPanel() override;

    /**
     * @brief Set the input element type to filter available reductions
     * @param element_type The element type of the pipeline input
     */
    void setInputType(std::type_index element_type);

    /**
     * @brief Get all configured pre-reduction entries
     */
    [[nodiscard]] std::vector<PreReductionEntry> const & entries() const { return _entries; }

    /**
     * @brief Clear all pre-reduction entries
     */
    void clearEntries();

    /**
     * @brief Add a pre-reduction entry
     * @param reduction_name The reduction name from RangeReductionRegistry
     * @param output_key The key for param binding reference
     * @return true if successfully added
     */
    bool addEntry(std::string const & reduction_name, std::string const & output_key);

    /**
     * @brief Rebuild all entries from a list of PreReductionStepDescriptors
     *
     * Used by JSON → UI loading (Phase 2). Clears existing entries, then adds
     * each entry from the descriptor's reduction_name and output_key fields.
     *
     * @param descriptors The pre-reduction descriptors from a parsed PipelineDescriptor
     * @return true if all entries were loaded successfully
     */
    bool loadFromDescriptors(
            std::vector<WhiskerToolbox::Transforms::V2::Examples::PreReductionStepDescriptor> const & descriptors);

signals:
    /**
     * @brief Emitted when pre-reductions change (add/remove)
     */
    void preReductionsChanged();

private slots:
    void onAddClicked();
    void onRemoveClicked();

private:
    void rebuildListDisplay();
    std::vector<std::string> getAvailableReductions() const;

    QGroupBox * _group_box = nullptr;
    QListWidget * _list_widget = nullptr;
    QPushButton * _add_button = nullptr;
    QPushButton * _remove_button = nullptr;

    std::vector<PreReductionEntry> _entries;
    std::type_index _input_element_type{typeid(void)};
};

#endif // WHISKERTOOLBOX_PRE_REDUCTION_PANEL_HPP
