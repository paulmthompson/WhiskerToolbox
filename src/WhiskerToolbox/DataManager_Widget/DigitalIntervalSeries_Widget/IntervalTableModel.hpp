#ifndef WHISKERTOOLBOX_INTERVALTABLEMODEL_HPP
#define WHISKERTOOLBOX_INTERVALTABLEMODEL_HPP

#include "DataManager/DigitalTimeSeries/interval_data.hpp"

#include <QAbstractTableModel>

#include <vector>

class IntervalTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit IntervalTableModel(QObject * parent = nullptr)
        : QAbstractTableModel(parent) {}

    void setIntervals(std::vector<Interval> const & intervals) {
        beginResetModel();
        _intervals = intervals;
        endResetModel();
    }

    [[nodiscard]] Interval getInterval(int row) const {
        return _intervals[row];
    }

    [[nodiscard]] int rowCount(QModelIndex const & parent) const override {
        Q_UNUSED(parent);
        return static_cast<int>(_intervals.size());
    }

    [[nodiscard]] int columnCount(QModelIndex const & parent) const override {
        Q_UNUSED(parent);
        return 2;// Start and End columns
    }

    [[nodiscard]] QVariant data(QModelIndex const & index, int role) const override {
        if (!index.isValid() || role != Qt::DisplayRole) {
            return QVariant{};
        }

        Interval const & interval = _intervals.at(index.row());
        if (index.column() == 0) {
            return QVariant::fromValue(interval.start);
        } else if (index.column() == 1) {
            return QVariant::fromValue(interval.end);
        }

        return QVariant{};
    }

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (role != Qt::DisplayRole) {
            return QVariant{};
        }

        if (orientation == Qt::Horizontal) {
            if (section == 0) {
                return QString("Start");
            } else if (section == 1) {
                return QString("End");
            }
        }

        return QVariant{};
    }

    [[nodiscard]] Qt::ItemFlags flags(QModelIndex const & index) const override {
        if (!index.isValid()) {
            return Qt::ItemIsEnabled;
        }
        return QAbstractTableModel::flags(index);
    }

private:
    std::vector<Interval> _intervals;
};

#endif//WHISKERTOOLBOX_INTERVALTABLEMODEL_HPP
