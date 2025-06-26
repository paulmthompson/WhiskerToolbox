#ifndef WHISKERTOOLBOX_POINTTABLEMODEL_HPP
#define WHISKERTOOLBOX_POINTTABLEMODEL_HPP

#include "DataManager/Points/Point_Data.hpp"

#include <QAbstractTableModel>

#include <map>
#include <sstream>
#include <vector>

class PointTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit PointTableModel(QObject * parent = nullptr)
        : QAbstractTableModel(parent) {}

    void setPoints(PointData const * pointData) {
        beginResetModel();
        _points.clear();// Clear previous data
        if (pointData) {// Check if pointData is not null
            for (auto const & timePointsPair: pointData->GetAllPointsAsRange()) {
                _points[timePointsPair.time] = timePointsPair.points;
            }
        }
        endResetModel();
    }

    [[nodiscard]] int rowCount(QModelIndex const & parent) const override {
        Q_UNUSED(parent);
        return static_cast<int>(_points.size());
    }

    [[nodiscard]] int columnCount(QModelIndex const & parent) const override {
        Q_UNUSED(parent);
        return 2;// Frame and Points columns
    }

    [[nodiscard]] QVariant data(QModelIndex const & index, int role) const override {
        if (!index.isValid() || role != Qt::DisplayRole) {
            return QVariant{};
        }

        auto it = std::next(_points.begin(), index.row());
        int const frame = it->first;
        std::vector<Point2D<float>> const & points = it->second;

        if (index.column() == 0) {
            return QVariant::fromValue(frame);
        } else if (index.column() == 1) {
            std::stringstream ss;
            for (auto const & point: points) {
                ss << "(" << point.x << "," << point.y << ") ";
            }
            return QString::fromStdString(ss.str());
        }

        return QVariant{};
    }

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (role != Qt::DisplayRole) {
            return QVariant{};
        }

        if (orientation == Qt::Horizontal) {
            if (section == 0) {
                return QString("Frame");
            } else if (section == 1) {
                return QString("Points");
            }
        }

        return QVariant{};
    }

    [[nodiscard]] int getFrameForRow(int row) const {
        if (row < 0 || static_cast<size_t>(row) >= _points.size()) {
            return -1;// Invalid row
        }
        auto it = std::next(_points.begin(), row);
        return it->first;// Return the frame number (the key in the map)
    }

private:
    std::map<int, std::vector<Point2D<float>>> _points;
};


#endif//WHISKERTOOLBOX_POINTTABLEMODEL_HPP
