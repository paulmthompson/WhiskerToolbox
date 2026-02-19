#ifndef LABEL_CONFIG_PANEL_HPP
#define LABEL_CONFIG_PANEL_HPP

/**
 * @file LabelConfigPanel.hpp
 * @brief Panel for configuring label sources in the ML workflow
 *
 * LabelConfigPanel provides radio buttons to select one of three labeling
 * modes and a dynamic form that adapts to the selected mode:
 *
 * 1. **Interval Series** (binary) — Select a DigitalIntervalSeries where
 *    frames inside intervals are the positive class and outside are negative.
 *    Class names are user-editable.
 *
 * 2. **Entity Groups** (multi-class via TimeEntity) — Select entity groups
 *    from the EntityGroupManager. Each group maps to one class label.
 *    Class names are derived from group names.
 *
 * 3. **Data Entity Groups** (multi-class via data objects) — Select a
 *    DataManager data key and entity groups. Entities are matched by
 *    data_key rather than time_key.
 *
 * The panel configures the appropriate fields in MLCoreWidgetState
 * (label_source_type, label_interval_key, label_group_ids, etc.)
 * which are later consumed by ClassificationPipeline via LabelAssembler.
 *
 * ## Integration
 *
 * - Embedded in the Classification tab of MLCoreWidget
 * - Reads/writes label configuration fields in MLCoreWidgetState
 * - Refreshes automatically when DataManager or EntityGroupManager changes
 * - Validates that the current configuration is sufficient for training
 *
 * @see MLCoreWidget for the parent widget
 * @see MLCoreWidgetState for the persisted label configuration
 * @see LabelAssembler for the three labeling modes
 * @see ClassificationPipeline for the pipeline that consumes labels
 */

#include <QWidget>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Forward declarations
class DataManager;
class MLCoreWidgetState;

namespace Ui {
class LabelConfigPanel;
}

class LabelConfigPanel : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct the label configuration panel
     *
     * @param state Shared MLCoreWidgetState for reading/writing label settings
     * @param data_manager Shared DataManager for listing interval series,
     *        data keys, and accessing EntityGroupManager
     * @param parent Optional parent widget
     */
    explicit LabelConfigPanel(std::shared_ptr<MLCoreWidgetState> state,
                              std::shared_ptr<DataManager> data_manager,
                              QWidget * parent = nullptr);

    ~LabelConfigPanel() override;

    /**
     * @brief Whether the current label configuration is valid for training
     *
     * - Interval mode: valid if an interval series is selected
     * - Group modes: valid if at least 2 groups are selected
     */
    [[nodiscard]] bool hasValidSelection() const;

    /**
     * @brief Get the current label source type
     *
     * @return "intervals", "groups", or "entity_groups"
     */
    [[nodiscard]] std::string labelSourceType() const;

    /**
     * @brief Get the currently selected group IDs (for group modes)
     *
     * @return Vector of GroupId values in class order (group[0] = class 0, etc.)
     */
    [[nodiscard]] std::vector<uint64_t> selectedGroupIds() const;

public slots:
    /**
     * @brief Refresh all combo boxes from current DataManager / group state
     *
     * Called automatically on DataManager or EntityGroupManager changes
     * and on refresh buttons. Preserves current selections if still available.
     */
    void refreshAll();

signals:
    /**
     * @brief Emitted when the label source type changes
     *
     * @param type The new label source type ("intervals", "groups", "entity_groups")
     */
    void labelSourceChanged(QString const & type);

    /**
     * @brief Emitted when selection validity changes
     *
     * @param valid Whether the current label configuration is valid
     */
    void validityChanged(bool valid);

private slots:
    void _onIntervalRadioToggled(bool checked);
    void _onGroupsRadioToggled(bool checked);
    void _onDataGroupsRadioToggled(bool checked);

    void _onIntervalComboChanged(int index);
    void _onPositiveClassNameEdited(QString const & text);
    void _onNegativeClassNameEdited(QString const & text);

    void _onAddGroupClicked();
    void _onRemoveGroupClicked();

    void _onDataKeyComboChanged(int index);
    void _onAddDataGroupClicked();
    void _onRemoveDataGroupClicked();

private:
    void _setupConnections();
    void _setupStateConnections();
    void _registerObservers();

    void _refreshIntervalCombo();
    void _refreshGroupCombo();
    void _refreshDataKeyCombo();
    void _refreshDataGroupCombo();

    void _rebuildClassList();
    void _rebuildDataClassList();

    void _updateIntervalInfo(std::string const & key);
    void _updateValidation();

    void _restoreFromState();
    void _syncSourceTypeToState();
    void _syncGroupIdsToState();

    /// Map radio button index to source type string
    [[nodiscard]] static int _sourceTypeToPageIndex(std::string const & type);

    Ui::LabelConfigPanel * ui;
    std::shared_ptr<MLCoreWidgetState> _state;
    std::shared_ptr<DataManager> _data_manager;

    int _dm_observer_id = -1;
    int _group_observer_id = -1;
    bool _valid = false;

    /// Currently selected group IDs for entity groups mode (page 1)
    std::vector<uint64_t> _selected_group_ids;

    /// Currently selected group IDs for data entity groups mode (page 2)
    std::vector<uint64_t> _selected_data_group_ids;
};

#endif // LABEL_CONFIG_PANEL_HPP
