#include "DataManager/utils/TableView/columns/ColumnTypeInfo.hpp"
#include <QVariant>
#include <QVariantList>
#include <QStringList>
#include <QString>

// Create a visitor class for converting TableView variants to QVariant
class QVariantColumnDataVisitor {
public:
    QVariant operator()(const std::vector<float>& data) {
        QVariantList result;
        for (float value : data) {
            result.append(value);
        }
        return QVariant::fromValue(result);
    }
    
    QVariant operator()(const std::vector<double>& data) {
        QVariantList result;
        for (double value : data) {
            result.append(value);
        }
        return QVariant::fromValue(result);
    }
    
    QVariant operator()(const std::vector<int>& data) {
        QVariantList result;
        for (int value : data) {
            result.append(value);
        }
        return QVariant::fromValue(result);
    }
    
    QVariant operator()(const std::vector<bool>& data) {
        QVariantList result;
        for (bool value : data) {
            result.append(value);
        }
        return QVariant::fromValue(result);
    }
    
    QVariant operator()(const std::vector<std::string>& data) {
        QStringList result;
        for (const auto& value : data) {
            result.append(QString::fromStdString(value));
        }
        return QVariant::fromValue(result);
    }
    
    QVariant operator()(const std::vector<std::vector<float>>& data) {
        QVariantList result;
        for (const auto& vector : data) {
            QVariantList inner_list;
            for (float value : vector) {
                inner_list.append(value);
            }
            result.append(QVariant::fromValue(inner_list));
        }
        return QVariant::fromValue(result);
    }
    
    QVariant operator()(const std::vector<std::vector<double>>& data) {
        QVariantList result;
        for (const auto& vector : data) {
            QVariantList inner_list;
            for (double value : vector) {
                inner_list.append(value);
            }
            result.append(QVariant::fromValue(inner_list));
        }
        return QVariant::fromValue(result);
    }
    
    QVariant operator()(const std::vector<std::vector<int>>& data) {
        QVariantList result;
        for (const auto& vector : data) {
            QVariantList inner_list;
            for (int value : vector) {
                inner_list.append(value);
            }
            result.append(QVariant::fromValue(inner_list));
        }
        return QVariant::fromValue(result);
    }
};
