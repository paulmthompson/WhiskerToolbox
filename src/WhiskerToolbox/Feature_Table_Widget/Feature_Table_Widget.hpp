#ifndef FEATURE_TABLE_WIDGET_HPP
#define FEATURE_TABLE_WIDGET_HPP

#include "DataManager/DataManagerTypes.hpp"

#include <QString>
#include <QStringList>
#include <QWidget>

#include <memory>
#include <string>
#include <vector>

class DataManager;
class QTableWidget;
class QPushButton;

namespace Ui {
class Feature_Table_Widget;
}

class Feature_Table_Widget : public QWidget {
    Q_OBJECT

public:
    explicit Feature_Table_Widget(QWidget * parent = nullptr);
    ~Feature_Table_Widget() override;

    void setDataManager(std::shared_ptr<DataManager> data_manager);

    void populateTable();
    void setColumns(QStringList columns) { _columns = std::move(columns); }
    void setTypeFilter(std::vector<DM_DataType> type) { _type_filters = std::move(type); }
    [[nodiscard]] QString getHighlightedFeature() const { return _highlighted_feature; }

signals:
    void featureSelected(QString const & feature);
    void addFeature(QString const & feature);
    void removeFeature(QString const & feature);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void _refreshFeatures();
    void _highlightFeature(int row, int column);

private:
    std::shared_ptr<DataManager> _data_manager;
    Ui::Feature_Table_Widget * ui;

    QString _highlighted_feature;
    QStringList _columns;
    std::vector<DM_DataType> _type_filters;

    void _addFeatureName(std::string const & key, int row, int col);
    void _addFeatureType(std::string const & key, int row, int col);
    void _addFeatureClock(std::string const & key, int row, int col);
    void _addFeatureElements(std::string const & key, int row, int col);
    void _addFeatureEnabled(std::string const & key, int row, int col);
};

#endif// FEATURE_TABLE_WIDGET_HPP
