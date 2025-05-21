#ifndef WHISKERTOOLBOX_LINETABLEMODEL_HPP
#define WHISKERTOOLBOX_LINETABLEMODEL_HPP

#include "DataManager/Lines/Line_Data.hpp" // Assuming Line_Data.hpp is the correct path
#include "DataManager/Points/points.hpp" // For Line2D definition

#include <QAbstractTableModel>
#include <vector>
#include <string>

// Forward declaration
class LineData;

struct LineTableRow {
    int frame;
    int lineIndex; // Index of the line within that frame
    int length;    // Number of points in the line
};

class LineTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit LineTableModel(QObject * parent = nullptr);

    void setLines(LineData const * lineData);

    [[nodiscard]] int rowCount(QModelIndex const & parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(QModelIndex const & parent = QModelIndex()) const override;

    [[nodiscard]] QVariant data(QModelIndex const & index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    [[nodiscard]] LineTableRow getRowData(int row) const;


private:
    std::vector<LineTableRow> _display_data;
    LineData const * _line_data_source{nullptr}; // Optional: direct pointer to source for more complex ops
};

#endif//WHISKERTOOLBOX_LINETABLEMODEL_HPP 