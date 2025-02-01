#ifndef WHISKERTOOLBOX_INTERVALTABLEMODEL_HPP
#define WHISKERTOOLBOX_INTERVALTABLEMODEL_HPP

#include "DataManager/DigitalTimeSeries/interval_data.hpp"

#include <QAbstractTableModel>

#include <vector>

class IntervalTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    IntervalTableModel(QObject *parent = nullptr) : QAbstractTableModel(parent) {}

    void setIntervals(const std::vector<Interval>& intervals) {
        beginResetModel();
        _intervals = intervals;
        endResetModel();
    }

    Interval getInterval(int row) const {
        return _intervals[row];
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return static_cast<int>(_intervals.size());
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return 2; // Start and End columns
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || role != Qt::DisplayRole) {
            return QVariant();
        }

        const Interval &interval = _intervals.at(index.row());
        if (index.column() == 0) {
            return QVariant::fromValue(interval.start);
        } else if (index.column() == 1) {
            return QVariant::fromValue(interval.end);
        }

        return QVariant();
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (role != Qt::DisplayRole) {
            return QVariant();
        }

        if (orientation == Qt::Horizontal) {
            if (section == 0) {
                return QString("Start");
            } else if (section == 1) {
                return QString("End");
            }
        }

        return QVariant();
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override {
        if (!index.isValid()) {
            return Qt::ItemIsEnabled;
        }
        return QAbstractTableModel::flags(index) | Qt::ItemIsEditable;
    }

    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override {
        if (index.isValid() && role == Qt::EditRole) {
            Interval &interval = _intervals[index.row()];
            if (index.column() == 0) {
                interval.start = value.toInt();
            } else if (index.column() == 1) {
                interval.end = value.toInt();
            }
            emit dataChanged(index, index, {role});
            return true;
        }
        return false;
    }


private:
    std::vector<Interval> _intervals;
};

#endif //WHISKERTOOLBOX_INTERVALTABLEMODEL_HPP
