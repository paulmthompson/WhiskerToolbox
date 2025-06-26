#ifndef WHISKERTOOLBOX_MASKTABLEMODEL_HPP
#define WHISKERTOOLBOX_MASKTABLEMODEL_HPP

#include <QAbstractTableModel>

#include <vector>

class MaskData;

struct MaskTableRow {
    int frame;
    int totalPointsInFrame;// Sum of points in all Mask2D objects at this frame
};

class MaskTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit MaskTableModel(QObject * parent = nullptr);

    void setMasks(MaskData const * maskData);

    [[nodiscard]] int rowCount(QModelIndex const & parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(QModelIndex const & parent = QModelIndex()) const override;

    [[nodiscard]] QVariant data(QModelIndex const & index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    [[nodiscard]] int getFrameForRow(int row) const;

private:
    std::vector<MaskTableRow> _display_data;
};

#endif//WHISKERTOOLBOX_MASKTABLEMODEL_HPP
