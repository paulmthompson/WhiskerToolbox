#ifndef WHISKERTOOLBOX_MASKTABLEMODEL_HPP
#define WHISKERTOOLBOX_MASKTABLEMODEL_HPP

#include <QAbstractTableModel>

#include "Entity/EntityTypes.hpp"

#include <cstdint>
#include <string>
#include <vector>

class MaskData;
class GroupManager;

struct MaskTableRow {
    int64_t frame;
    int totalPointsInFrame; // Sum of points in all Mask2D objects at this frame
    EntityId entity_id; // EntityId for group lookup
    QString group_name; // Name of the group this mask belongs to
};

class MaskTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit MaskTableModel(QObject * parent = nullptr);

    void setMasks(MaskData const * maskData);
    void setGroupManager(GroupManager * group_manager);
    void setGroupFilter(int group_id); // -1 means show all groups
    void clearGroupFilter();

    [[nodiscard]] int rowCount(QModelIndex const & parent) const override;
    [[nodiscard]] int columnCount(QModelIndex const & parent) const override;

    [[nodiscard]] QVariant data(QModelIndex const & index, int role) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    [[nodiscard]] int getFrameForRow(int row) const;
    [[nodiscard]] MaskTableRow getRowData(int row) const;

private:
    std::vector<MaskTableRow> _display_data;
    std::vector<MaskTableRow> _all_data; // Store all data for filtering
    MaskData const * _mask_data_source{nullptr};
    GroupManager * _group_manager{nullptr};
    int _filtered_group_id{-1}; // -1 means show all groups
    
    void _applyGroupFilter();
};

#endif//WHISKERTOOLBOX_MASKTABLEMODEL_HPP
