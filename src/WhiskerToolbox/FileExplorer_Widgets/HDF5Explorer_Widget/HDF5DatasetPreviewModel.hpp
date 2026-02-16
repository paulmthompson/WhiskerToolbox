#ifndef HDF5_DATASET_PREVIEW_MODEL_HPP
#define HDF5_DATASET_PREVIEW_MODEL_HPP

/**
 * @file HDF5DatasetPreviewModel.hpp
 * @brief Lazy-loading table model for previewing HDF5 dataset contents
 * 
 * This model only loads data chunks as they become visible in the view,
 * making it efficient for large datasets. It uses HDF5 hyperslab selection
 * to read only the requested portion of data.
 */

#include <QAbstractTableModel>
#include <QString>
#include <QCache>
#include <QVariant>

#include <memory>
#include <vector>
#include <optional>
#include <cstdint>

// Forward declare hsize_t to avoid pulling in H5Cpp.h in header
using hsize_t = std::uint64_t;

/**
 * @brief Cached chunk of data for the preview model
 */
struct HDF5PreviewDataChunk {
    qint64 start_row;
    std::vector<std::vector<QVariant>> rows;
};

/**
 * @brief Lazy-loading table model for HDF5 dataset preview
 * 
 * This model presents HDF5 dataset data in a table format:
 * - 1D datasets: Single column with index as row
 * - 2D datasets: Rows and columns map directly
 * - ND datasets: Flattened view with multi-dimensional index shown
 * 
 * Data is loaded in chunks (default 100 rows) and cached using QCache
 * to provide smooth scrolling while minimizing memory usage.
 */
class HDF5DatasetPreviewModel : public QAbstractTableModel {
    Q_OBJECT

public:
    /**
     * @brief Construct the preview model
     * @param parent Parent object
     */
    explicit HDF5DatasetPreviewModel(QObject * parent = nullptr);
    
    ~HDF5DatasetPreviewModel() override;

    /**
     * @brief Load a dataset from an HDF5 file for preview
     * 
     * @param file_path Path to the HDF5 file
     * @param dataset_path Path to the dataset within the file
     * @return true if dataset was loaded successfully
     */
    bool loadDataset(QString const & file_path, QString const & dataset_path);

    /**
     * @brief Clear the current dataset
     */
    void clear();

    /**
     * @brief Check if a dataset is currently loaded
     */
    [[nodiscard]] bool hasDataset() const { return _has_dataset; }

    /**
     * @brief Get the total number of elements in the dataset
     */
    [[nodiscard]] qint64 totalElements() const { return _total_elements; }

    /**
     * @brief Get the chunk size used for lazy loading
     */
    [[nodiscard]] int chunkSize() const { return _chunk_size; }

    /**
     * @brief Set the chunk size for lazy loading
     * @param size Number of rows to load at once (minimum 10)
     */
    void setChunkSize(int size);

    // QAbstractTableModel interface
    [[nodiscard]] int rowCount(QModelIndex const & parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(QModelIndex const & parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(QModelIndex const & index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, 
                                       int role = Qt::DisplayRole) const override;

signals:
    /**
     * @brief Emitted when dataset loading fails
     * @param message Error message
     */
    void loadError(QString const & message);

    /**
     * @brief Emitted when dataset is successfully loaded
     * @param num_rows Number of rows in the view
     * @param num_cols Number of columns in the view
     */
    void datasetLoaded(qint64 num_rows, int num_cols);

private:
    /**
     * @brief Load a chunk of data starting at the given row
     * @param start_row First row to load
     * @return Pointer to loaded chunk, or nullptr on failure
     */
    HDF5PreviewDataChunk * loadChunk(qint64 start_row) const;

    /**
     * @brief Get the chunk index for a given row
     */
    [[nodiscard]] qint64 chunkIndexForRow(qint64 row) const;

    /**
     * @brief Read data from HDF5 file for the specified row range
     * @param start_row First row to read
     * @param num_rows Number of rows to read
     * @return Vector of row data, or empty on failure
     */
    std::vector<std::vector<QVariant>> readHDF5Data(qint64 start_row, qint64 num_rows) const;

    // Dataset metadata
    QString _file_path;
    QString _dataset_path;
    bool _has_dataset = false;
    
    // Dimensions for display
    qint64 _num_rows = 0;      // Number of rows in table view
    int _num_cols = 0;         // Number of columns in table view
    qint64 _total_elements = 0;
    
    // Original HDF5 dimensions
    std::vector<hsize_t> _dimensions;
    int _type_class = -1;      // H5T_class_t value
    size_t _type_size = 0;
    bool _is_signed = true;
    bool _is_string = false;
    
    // Chunk loading configuration
    int _chunk_size = 100;     // Rows per chunk
    
    // Cache for loaded chunks (mutable for lazy loading in const methods)
    mutable QCache<qint64, HDF5PreviewDataChunk> _chunk_cache;
};

#endif // HDF5_DATASET_PREVIEW_MODEL_HPP
