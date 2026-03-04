#ifndef TIMEFRAME_INSPECTOR_HPP
#define TIMEFRAME_INSPECTOR_HPP

/**
 * @file TimeFrameInspector.hpp
 * @brief Inspector widget for TimeFrame objects
 * 
 * TimeFrameInspector provides inspection capabilities for TimeFrame objects.
 * TimeFrames are temporal time series that define the time base for data objects.
 * 
 * ## Features
 * - Displays the TimeFrame key and sample count
 * - Group filter combo box to filter the view table by group membership
 * - Auto-updates when groups are added/removed/modified
 * 
 * @see BaseInspector for the base class
 * @see TimeFrameDataView for the table view
 */

#include "DataInspector_Widget/Inspectors/BaseInspector.hpp"

class QComboBox;
class QLabel;
class TimeFrameDataView;

/**
 * @brief Inspector widget for TimeFrame objects
 * 
 * Provides property display and group filtering for TimeFrame data inspection.
 * Unlike data inspectors, this works with TimeFrame keys rather than data keys.
 */
class TimeFrameInspector : public BaseInspector {
    Q_OBJECT

public:
    /**
     * @brief Construct the TimeFrame inspector
     * @param data_manager Shared DataManager for data access
     * @param group_manager Optional GroupManager for group filtering
     * @param parent Parent widget
     */
    explicit TimeFrameInspector(
        std::shared_ptr<DataManager> data_manager,
        GroupManager * group_manager = nullptr,
        QWidget * parent = nullptr);

    ~TimeFrameInspector() override;

    // =========================================================================
    // IDataInspector Interface
    // =========================================================================

    void setActiveKey(std::string const & key) override;
    void removeCallbacks() override;
    void updateView() override;

    [[nodiscard]] DM_DataType getDataType() const override { return DM_DataType::Time; }
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("TimeFrame"); }

    [[nodiscard]] bool supportsExport() const override { return false; }
    [[nodiscard]] bool supportsGroupFiltering() const override { return true; }

    /**
     * @brief Set the data view to use for group filtering
     * @param view Pointer to the TimeFrameDataView (can be nullptr)
     */
    void setDataView(TimeFrameDataView * view);

private slots:
    void _onGroupFilterChanged(int index);
    void _onGroupChanged();

private:
    void _setupUi();
    void _connectSignals();
    void _populateGroupFilterCombo();

    QLabel * _info_label{nullptr};
    QComboBox * _group_filter_combo{nullptr};
    TimeFrameDataView * _data_view{nullptr};
};

#endif // TIMEFRAME_INSPECTOR_HPP
