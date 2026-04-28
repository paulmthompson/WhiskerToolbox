/// @file DataSourceComboHelper.hpp
/// @brief Centralized helper for populating and refreshing DataManager-aware
///        QComboBox widgets.
///
/// Extracts combo population logic from DeepLearningPropertiesWidget into a
/// reusable utility. Combos can be registered for automatic refresh when
/// DataManager keys change.

#ifndef DATA_SOURCE_COMBO_HELPER_HPP
#define DATA_SOURCE_COMBO_HELPER_HPP

#include "DataTypeEnum/DM_DataType.hpp"

#include <QObject>

#include <memory>
#include <string>
#include <vector>

class DataManager;
class QComboBox;

namespace dl::widget {

/**
 * @brief Manages DataManager-aware QComboBox population and auto-refresh.
 *
 * Tracks registered combos and the DM_DataType filter each should display.
 * When refreshAll() is called (typically from a DataManager observer), all
 * tracked combos are re-populated while preserving the user's current
 * selection where possible.
 *
 * @note Not thread-safe — must only be used from the GUI thread.
 */
class DataSourceComboHelper : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct a helper bound to a DataManager.
     * @param dm  Shared pointer to the DataManager to query for keys.
     * @param parent  Optional QObject parent for lifetime management.
     *
     * @pre dm must not be null.
     */
    explicit DataSourceComboHelper(
            std::shared_ptr<DataManager> dm,
            QObject * parent = nullptr);

    ~DataSourceComboHelper() override;

    // Non-copyable, non-movable (QObject parent-child ownership)
    DataSourceComboHelper(DataSourceComboHelper const &) = delete;
    DataSourceComboHelper & operator=(DataSourceComboHelper const &) = delete;
    DataSourceComboHelper(DataSourceComboHelper &&) = delete;
    DataSourceComboHelper & operator=(DataSourceComboHelper &&) = delete;

    // ── One-shot population ────────────────────────────────────────────────

    /**
     * @brief Populate a combo with DataManager keys matching the given types.
     *
     * Clears the combo, adds a "(None)" sentinel, then appends all matching
     * keys. If the combo previously had a valid selection, it is restored.
     *
     * @param combo  The combo box to populate.
     * @param types  Data types to include. An empty vector means all keys.
     *
     * @pre combo must not be null.
     */
    void populateCombo(QComboBox * combo,
                       std::vector<DM_DataType> const & types) const;

    /**
     * @brief Convenience overload to populate from a single data type.
     */
    void populateCombo(QComboBox * combo, DM_DataType type) const;

    // ── Tracking for auto-refresh ──────────────────────────────────────────

    /**
     * @brief Register a combo for automatic refresh.
     *
     * The combo will be re-populated with the given type filter whenever
     * refreshAll() is called. If the combo is destroyed before being
     * untracked, it is automatically removed.
     *
     * @param combo  The combo box to track.
     * @param types  Data types to include on refresh. Empty = all keys.
     *
     * @pre combo must not be null.
     */
    void track(QComboBox * combo, std::vector<DM_DataType> const & types);

    /**
     * @brief Convenience overload to track with a single data type.
     */
    void track(QComboBox * combo, DM_DataType type);

    /**
     * @brief Stop tracking a combo for auto-refresh.
     *
     * Safe to call even if the combo was never tracked or already untracked.
     */
    void untrack(QComboBox * combo);

    /**
     * @brief Re-populate all tracked combos.
     *
     * Typically connected to a DataManager observer callback.
     */
    void refreshAll();

    /**
     * @brief Return the number of currently tracked combos.
     */
    [[nodiscard]] int trackedCount() const;

    // ── Utility ────────────────────────────────────────────────────────────

    /**
     * @brief Convert a type-hint string to a vector of DM_DataType values.
     *
     * Maps known strings: "MediaData" → {Video, Images},
     * "PointData" → {Points}, "MaskData" → {Mask}, "LineData" → {Line},
     * "AnalogTimeSeries" → {Analog}, "TensorData" → {Tensor}.
     *
     * Returns an empty vector for unrecognized strings (meaning "all keys").
     */
    [[nodiscard]] static std::vector<DM_DataType>
    typesFromHint(std::string const & type_hint);

private:
    struct TrackedCombo {
        QComboBox * combo = nullptr;
        std::vector<DM_DataType> types;
    };

    /// Collect matching keys from the DataManager.
    [[nodiscard]] std::vector<std::string>
    keysForTypes(std::vector<DM_DataType> const & types) const;

    std::shared_ptr<DataManager> _dm;
    std::vector<TrackedCombo> _tracked;
};

}// namespace dl::widget

#endif// DATA_SOURCE_COMBO_HELPER_HPP
