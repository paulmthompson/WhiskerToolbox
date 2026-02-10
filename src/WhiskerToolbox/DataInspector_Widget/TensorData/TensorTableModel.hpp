#ifndef TENSORTABLEMODEL_HPP
#define TENSORTABLEMODEL_HPP

/**
 * @file TensorTableModel.hpp
 * @brief Qt table model for N-dimensional TensorData with user-selectable row/column dimensions
 *
 * The model lets the user choose which tensor axis maps to table rows and which
 * maps to table columns. All remaining dimensions use a fixed slice index
 * (controllable via setFixedIndex). Data is fetched lazily — Qt only calls
 * data() for visible cells, so even very large tensors stay responsive.
 *
 * For 1-D tensors the column dimension is implicitly "none" and a single
 * "Value" column is shown. For 0-D (scalar) tensors one row × one column is
 * displayed.
 */

#include <QAbstractTableModel>

#include <cstddef>
#include <string>
#include <vector>

class TensorData;

class TensorTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit TensorTableModel(QObject * parent = nullptr);

    // ========== Data binding ==========

    /**
     * @brief Bind (or unbind) the model to a TensorData object
     *
     * Resets the model and auto-selects sensible row/column dimensions
     * (row = 0, col = 1 if ndim >= 2).
     */
    void setTensorData(TensorData * tensor_data);

    // ========== Dimension mapping ==========

    /**
     * @brief Set which tensor axis maps to table rows
     * @param dim Axis index (0-based). Must be < ndim() and != columnDimension()
     */
    void setRowDimension(int dim);

    /**
     * @brief Set which tensor axis maps to table columns
     * @param dim Axis index (0-based), or -1 for "no column dimension" (single value column).
     *            Must be != rowDimension()
     */
    void setColumnDimension(int dim);

    /**
     * @brief Set the fixed slice index for a dimension that is neither row nor column
     * @param dim Axis index
     * @param index Slice index (must be < axis size)
     */
    void setFixedIndex(int dim, std::size_t index);

    // ========== Queries ==========

    [[nodiscard]] int rowDimension() const noexcept { return _row_dim; }
    [[nodiscard]] int columnDimension() const noexcept { return _col_dim; }
    [[nodiscard]] std::size_t fixedIndex(int dim) const;

    /// Number of dimensions of the bound tensor (0 if unbound)
    [[nodiscard]] std::size_t ndim() const noexcept;

    /// Shape of the bound tensor (empty if unbound)
    [[nodiscard]] std::vector<std::size_t> tensorShape() const;

    /// Axis names of the bound tensor
    [[nodiscard]] std::vector<std::string> axisNames() const;

    // ========== QAbstractTableModel overrides ==========

    [[nodiscard]] int rowCount(QModelIndex const & parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(QModelIndex const & parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(QModelIndex const & index, int role) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    /// Rebuild _fixed_indices vector and pick default row/col dims
    void _resetDimensionMapping();

    TensorData * _tensor_data{nullptr};
    int _row_dim{0};            ///< Axis index shown as table rows
    int _col_dim{-1};           ///< Axis index shown as table columns (-1 = single value column)
    std::vector<std::size_t> _fixed_indices;  ///< One per tensor axis; only entries for "other" dims matter
};

#endif // TENSORTABLEMODEL_HPP
