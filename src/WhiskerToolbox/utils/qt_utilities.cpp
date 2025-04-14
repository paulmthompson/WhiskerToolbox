
#include <QStringList>
#include <QTableWidget>

void build_feature_table(QTableWidget* table_widget)
{
    table_widget->clear();
    QStringList const headers = {"Feature"};
    table_widget->setColumnCount(1);
    table_widget->setHorizontalHeaderLabels(headers);
}
