#ifndef FEATURE_TABLE_WIDGET_HPP
#define FEATURE_TABLE_WIDGET_HPP

#include <QWidget>
#include <QString>
#include <QStringList>

#include <memory>
#include <string>
#include <vector>

class DataManager;
class QTableWidget;
class QPushButton;

namespace Ui { class Feature_Table_Widget; }

class Feature_Table_Widget : public QWidget {
    Q_OBJECT

public:
    Feature_Table_Widget(QWidget *parent = nullptr);
    virtual ~Feature_Table_Widget();
    void setDataManager(std::shared_ptr<DataManager> data_manager);

    void populateTable();
    void setColumns(QStringList columns) { _columns = columns; }
    void setTypeFilter(std::vector<std::string> type) { _type_filters = type; }
    std::string getFeatureColor(std::string key);
    void setFeatureColor(std::string key, std::string hex_color);

signals:
    void featureSelected(const QString& feature);
    void addFeature(const QString& feature);
    void removeFeature(const QString& feature);
    void colorChange(const QString& feature, const QString& hex_color);

private slots:
    void _refreshFeatures();
    void _highlightFeature(int row, int column);

private:
    std::shared_ptr<DataManager> _data_manager;
    Ui::Feature_Table_Widget *ui;

    QString _highlighted_feature;
    QStringList _columns;
    std::vector<std::string> _type_filters;

    void _addFeatureName(std::string key, int row, int col, bool group);
    void _addFeatureType(std::string key, int row, int col, bool group);
    void _addFeatureClock(std::string key, int row, int col, bool group);
    void _addFeatureElements(std::string key, int row, int col, bool group);
    void _addFeatureEnabled(std::string key, int row, int col, bool group);
    void _addFeatureColor(std::string key, int row, int col, bool group);

};

#endif // FEATURE_TABLE_WIDGET_HPP
