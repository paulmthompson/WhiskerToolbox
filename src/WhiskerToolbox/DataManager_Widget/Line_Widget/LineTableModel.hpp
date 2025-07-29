#ifndef WHISKERTOOLBOX_LINETABLEMODEL_HPP
#define WHISKERTOOLBOX_LINETABLEMODEL_HPP

#include <QAbstractTableModel>

#include <cstdint>
#include <string>
#include <vector>


class LineData;

struct LineTableRow {
    int64_t frame;
    int lineIndex;// Index of the line within that frame
    int length;   // Number of points in the line
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
    LineData const * _line_data_source{nullptr};
};

#endif//WHISKERTOOLBOX_LINETABLEMODEL_HPP