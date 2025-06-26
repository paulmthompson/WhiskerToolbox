#include "MaskTableModel.hpp"

#include "DataManager/Masks/Mask_Data.hpp"

#include <iostream>// For debugging

MaskTableModel::MaskTableModel(QObject * parent)
    : QAbstractTableModel(parent) {}

void MaskTableModel::setMasks(MaskData const * maskData) {
    beginResetModel();
    _display_data.clear();
    if (maskData) {
        for (auto const & timeMaskPair: maskData->getAllAsRange()) {
            int frame = timeMaskPair.time;
            int totalPoints = 0;
            for (auto const & mask: timeMaskPair.masks) {
                totalPoints += static_cast<int>(mask.size());
            }
            _display_data.push_back({frame, totalPoints});
        }
    }
    endResetModel();
}

int MaskTableModel::rowCount(QModelIndex const & parent) const {
    Q_UNUSED(parent);
    return static_cast<int>(_display_data.size());
}

int MaskTableModel::columnCount(QModelIndex const & parent) const {
    Q_UNUSED(parent);
    return 2;// Frame, Total Points
}

QVariant MaskTableModel::data(QModelIndex const & index, int role) const {
    if (!index.isValid() || role != Qt::DisplayRole) {
        return QVariant{};
    }

    if (index.row() >= _display_data.size() || index.row() < 0) {
        return QVariant{};
    }

    MaskTableRow const & rowData = _display_data.at(index.row());

    switch (index.column()) {
        case 0:
            return QVariant::fromValue(rowData.frame);
        case 1:
            return QVariant::fromValue(rowData.totalPointsInFrame);
        default:
            return QVariant{};
    }
}

QVariant MaskTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) {
        return QVariant{};
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
            case 0:
                return QString("Frame");
            case 1:
                return QString("Total Points in Frame");
            default:
                return QVariant{};
        }
    }
    return QVariant{};// No vertical header
}

int MaskTableModel::getFrameForRow(int row) const {
    if (row >= 0 && static_cast<size_t>(row) < _display_data.size()) {
        return _display_data[row].frame;
    }
    return -1;// Invalid row
}
