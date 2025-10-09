#ifndef WHISKERTOOLBOX_POINTTABLEMODEL_HPP
#define WHISKERTOOLBOX_POINTTABLEMODEL_HPP

#include "DataManager/Points/Point_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "Entity/EntityTypes.hpp"

#include <QAbstractTableModel>

#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <vector>

class PointData;
class GroupManager;

struct PointTableRow {
    int64_t frame;
    int pointCount; // Number of points in this frame
    EntityId entity_id; // EntityId for group lookup (using first point's entity ID)
    QString group_name; // Name of the group this frame belongs to
};

class PointTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit PointTableModel(QObject * parent = nullptr);

    void setPoints(PointData const * pointData);
    void setGroupManager(GroupManager * group_manager);
    void setGroupFilter(int group_id); // -1 means show all groups
    void clearGroupFilter();

    [[nodiscard]] int rowCount(QModelIndex const & parent) const override;
    [[nodiscard]] int columnCount(QModelIndex const & parent) const override;

    [[nodiscard]] QVariant data(QModelIndex const & index, int role) const override;

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    [[nodiscard]] int getFrameForRow(int row) const;
    [[nodiscard]] PointTableRow getRowData(int row) const;

private:
    std::vector<PointTableRow> _display_data;
    std::vector<PointTableRow> _all_data; // Store all data for filtering
    PointData const * _point_data_source{nullptr};
    GroupManager * _group_manager{nullptr};
    int _filtered_group_id{-1}; // -1 means show all groups
    
    void _applyGroupFilter();
};


#endif//WHISKERTOOLBOX_POINTTABLEMODEL_HPP
