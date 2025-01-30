
#ifndef WHISKERTOOLBOX_POINTTABLEMODEL_HPP
#define WHISKERTOOLBOX_POINTTABLEMODEL_HPP

#include "DataManager/Points/points.hpp"

#include <QAbstractTableModel>

#include <map>
#include <sstream>
#include <vector>

class PointTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    PointTableModel(QObject *parent = nullptr) : QAbstractTableModel(parent) {}

    void setPoints(const std::map<int, std::vector<Point2D<float>>>& points) {
        beginResetModel();
        _points = points;
        endResetModel();
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return static_cast<int>(_points.size());
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return 2; // Frame and Points columns
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || role != Qt::DisplayRole) {
            return QVariant();
        }

        auto it = std::next(_points.begin(), index.row());
        const int frame = it->first;
        const std::vector<Point2D<float>> &points = it->second;

        if (index.column() == 0) {
            return QVariant::fromValue(frame);
        } else if (index.column() == 1) {
            std::stringstream ss;
            for (const auto &point : points) {
                ss << "(" << point.x << "," << point.y << ") ";
            }
            return QString::fromStdString(ss.str());
        }

        return QVariant();
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (role != Qt::DisplayRole) {
            return QVariant();
        }

        if (orientation == Qt::Horizontal) {
            if (section == 0) {
                return QString("Frame");
            } else if (section == 1) {
                return QString("Points");
            }
        }

        return QVariant();
    }

private:
    std::map<int, std::vector<Point2D<float>>> _points;
};


#endif //WHISKERTOOLBOX_POINTTABLEMODEL_HPP
