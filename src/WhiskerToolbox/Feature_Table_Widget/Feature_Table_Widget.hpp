#ifndef FEATURE_TABLE_WIDGET_HPP
#define FEATURE_TABLE_WIDGET_HPP

#include <QWidget>
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

    void populateTable(const std::vector<std::string>& keys);

signals:
    void featureSelected(const QString& feature);
    void addFeature(const QString& feature);

private slots:
    void _refreshFeatures();
    void _highlightFeature(int row, int column);
    void _addFeature();

private:
    std::shared_ptr<DataManager> _data_manager;
    Ui::Feature_Table_Widget *ui;

    QString _highlighted_feature;
};

#endif // FEATURE_TABLE_WIDGET_HPP
