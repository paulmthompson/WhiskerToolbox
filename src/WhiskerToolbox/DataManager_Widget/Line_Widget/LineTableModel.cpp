#include "LineTableModel.hpp"
#include "DataManager/Lines/Line_Data.hpp"

#include <iostream>// For debugging

LineTableModel::LineTableModel(QObject * parent)
    : QAbstractTableModel(parent) {}

void LineTableModel::setLines(LineData const * lineData) {
    beginResetModel();
    _display_data.clear();
    _line_data_source = lineData;
    if (lineData) {
        for (auto const & timeLinesPair: lineData->GetAllLinesAsRange()) {
            int frame = timeLinesPair.time;
            int lineIndex = 0;
            for (auto const & line: timeLinesPair.lines) {
                _display_data.push_back({frame, lineIndex, static_cast<int>(line.size())});
                lineIndex++;
            }
        }
    }
    endResetModel();
}

int LineTableModel::rowCount(QModelIndex const & parent) const {
    Q_UNUSED(parent);
    return static_cast<int>(_display_data.size());
}

int LineTableModel::columnCount(QModelIndex const & parent) const {
    Q_UNUSED(parent);
    return 3;// Frame, Line Index, Length
}

QVariant LineTableModel::data(QModelIndex const & index, int role) const {
    if (!index.isValid() || role != Qt::DisplayRole) {
        return QVariant{};
    }

    if (index.row() >= _display_data.size()) {
        return QVariant{};
    }

    LineTableRow const & rowData = _display_data.at(index.row());

    switch (index.column()) {
        case 0:
            return QVariant::fromValue(rowData.frame);
        case 1:
            return QVariant::fromValue(rowData.lineIndex);
        case 2:
            return QVariant::fromValue(rowData.length);
        default:
            return QVariant{};
    }
}

QVariant LineTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) {
        return QVariant{};
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
            case 0:
                return QString("Frame");
            case 1:
                return QString("Line Index");
            case 2:
                return QString("Length (points)");
            default:
                return QVariant{};
        }
    }
    return QVariant{};// No vertical header
}

LineTableRow LineTableModel::getRowData(int row) const {
    if (row >= 0 && row < _display_data.size()) {
        return _display_data[row];
    }
    // Return a default/invalid LineTableRow or throw an exception
    // For simplicity, returning a default-constructed one here, but error handling might be better.
    return LineTableRow{-1, -1, -1};
}