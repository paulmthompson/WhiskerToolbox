#ifndef WHISKERTOOLBOX_LINETABLEMODEL_HPP
#define WHISKERTOOLBOX_LINETABLEMODEL_HPP

#include <QAbstractTableModel>

#include "Entity/EntityTypes.hpp"

#include <cstdint>
#include <string>
#include <vector>


class LineData;
class GroupManager;

struct LineTableRow {
    int64_t frame;
    int lineIndex;     // Index of the line within that frame
    int length;        // Number of points in the line
    EntityId entity_id;// EntityId for group lookup
    QString group_name;// Name of the group this line belongs to
};

class LineTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit LineTableModel(QObject * parent = nullptr);

    void setLines(LineData const * lineData);
    void setGroupManager(GroupManager * group_manager);
    void setGroupFilter(int group_id);// -1 means show all groups
    void clearGroupFilter();

    [[nodiscard]] int rowCount(QModelIndex const & parent) const override;
    [[nodiscard]] int columnCount(QModelIndex const & parent) const override;

    [[nodiscard]] QVariant data(QModelIndex const & index, int role) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    [[nodiscard]] LineTableRow getRowData(int row) const;

    /**
     * @brief Find the first row containing data for the specified frame
     * 
     * @param frame The frame number to search for
     * @return The row index of the first line at the specified frame, or -1 if not found
     */
    [[nodiscard]] int findRowForFrame(int64_t frame) const;


private:
    std::vector<LineTableRow> _display_data;
    std::vector<LineTableRow> _all_data;// Store all data for filtering
    LineData const * _line_data_source{nullptr};
    GroupManager * _group_manager{nullptr};
    int _filtered_group_id{-1};// -1 means show all groups

    void _applyGroupFilter();
};

#endif//WHISKERTOOLBOX_LINETABLEMODEL_HPP