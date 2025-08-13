#ifndef PREVIEWTABLEMODEL_HPP
#define PREVIEWTABLEMODEL_HPP

#include "DataManager/utils/TableView/core/TableView.h"
#include <QAbstractTableModel>
#include <QVariant>
#include <QStringList>

#include <optional>
#include <string>
#include <vector>

// QAbstractTableModel included above

/**
 * @brief Read-only table model that renders a small-window TableView preview.
 *
 * Renders scalar columns directly and joins vector elements using commas
 * (e.g., 1,2,3) per the user's preference.
 */
class PreviewTableModel : public QAbstractTableModel {

public:
    explicit PreviewTableModel(QObject * parent = nullptr);
    ~PreviewTableModel() override = default;

    /**
     * @brief Replace the current preview with a new TableView.
     * @param view Newly built preview TableView.
     */
    void setPreview(std::shared_ptr<TableView> view);

    /**
     * @brief Clear any existing preview.
     */
    void clearPreview();

    // QAbstractTableModel API
    [[nodiscard]] int rowCount(QModelIndex const & parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(QModelIndex const & parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(QModelIndex const & index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    std::shared_ptr<TableView> _preview_view;
    QStringList _column_names;

    static QString formatScalarBool(bool value);
    static QString formatScalarInt(int value);
    static QString formatScalarDouble(double value);

    template<typename T>
    static QString joinVector(std::vector<T> const & values);
};

#endif // PREVIEWTABLEMODEL_HPP


