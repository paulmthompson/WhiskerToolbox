#ifndef DATAVIEW_IDATAVIEWTRANSFORM_HPP
#define DATAVIEW_IDATAVIEWTRANSFORM_HPP

#include <QString>
#include <memory>
#include <optional>
#include <vector>

// Forward declarations
class TableView;
class TableRegistry;

/**
 * @brief Execution context passed to data view transforms
 */
struct DataViewContext {
    QString tableId;
    std::shared_ptr<TableView> tableView;
    TableRegistry * tableRegistry{nullptr};
    size_t rowCount{0};
};

/**
 * @brief Mutable state produced by transforms and consumed by widgets
 */
struct DataViewState {
    std::vector<uint8_t> rowMask;                 // 1 = keep, 0 = drop
    std::vector<size_t> rowOrder;                 // permutation of [0..N-1]
    std::optional<std::vector<uint32_t>> rowColorIndices; // optional per-row color index
};

/**
 * @brief Initialize a default DataViewState for given row count
 */
inline DataViewState makeDefaultDataViewState(size_t rowCount) {
    DataViewState s;
    s.rowMask.assign(rowCount, static_cast<uint8_t>(1));
    s.rowOrder.resize(rowCount);
    for (size_t i = 0; i < rowCount; ++i) s.rowOrder[i] = i;
    return s;
}

/**
 * @brief Base interface for all row-level data transforms
 */
class IDataViewTransform {
public:
    virtual ~IDataViewTransform() = default;

    /**
     * @brief A stable identifier for the transform
     */
    virtual QString id() const = 0;

    /**
     * @brief Human-readable name
     */
    virtual QString displayName() const = 0;

    /**
     * @brief Apply the transform to the given state in the provided context
     * @return true on success, false on validation failure
     */
    virtual bool apply(DataViewContext const & context, DataViewState & state) = 0;
};

#endif // DATAVIEW_IDATAVIEWTRANSFORM_HPP

