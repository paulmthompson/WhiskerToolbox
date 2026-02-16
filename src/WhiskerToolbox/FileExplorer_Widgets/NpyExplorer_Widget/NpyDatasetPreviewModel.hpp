#ifndef NPY_DATASET_PREVIEW_MODEL_HPP
#define NPY_DATASET_PREVIEW_MODEL_HPP

/**
 * @file NpyDatasetPreviewModel.hpp
 * @brief Lazy-loading table model for previewing NumPy .npy file contents
 * 
 * This model only loads data chunks as they become visible in the view,
 * making it efficient for large arrays. It parses the .npy header to
 * determine shape and dtype, then reads only the requested portion of data.
 */

#include <QAbstractTableModel>
#include <QCache>
#include <QString>
#include <QVariant>

#include <cstdint>
#include <memory>
#include <vector>

/**
 * @brief Cached chunk of data for the preview model
 */
struct NpyPreviewDataChunk {
    qint64 start_row;
    std::vector<std::vector<QVariant>> rows;
};

/**
 * @brief Metadata parsed from a .npy file header
 */
struct NpyFileInfo {
    QString dtype_str;               ///< NumPy dtype string (e.g. "<f4", "<i8")
    char byte_order = '<';           ///< '<' little-endian, '>' big-endian, '|' not applicable
    char type_kind = 'f';            ///< 'f' float, 'i' signed int, 'u' unsigned int, 'b' bool
    unsigned int item_size = 4;      ///< Size of each element in bytes
    bool fortran_order = false;      ///< Whether data is in Fortran (column-major) order
    std::vector<uint64_t> shape;     ///< Shape of the array
    qint64 total_elements = 0;       ///< Total number of elements
    qint64 data_offset = 0;          ///< Byte offset where data begins in the file
};

/**
 * @brief Lazy-loading table model for NumPy .npy file preview
 * 
 * This model presents .npy array data in a table format:
 * - 1D arrays: Single column with index as row
 * - 2D arrays: Rows and columns map directly
 * - ND arrays: First dimension is rows, remaining dimensions flattened to columns
 * 
 * Data is loaded in chunks (default 100 rows) and cached using QCache
 * to provide smooth scrolling while minimizing memory usage.
 */
class NpyDatasetPreviewModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit NpyDatasetPreviewModel(QObject * parent = nullptr);
    ~NpyDatasetPreviewModel() override;

    /**
     * @brief Load a .npy file for preview
     * 
     * @param file_path Path to the .npy file
     * @return true if file was loaded successfully
     */
    bool loadFile(QString const & file_path);

    /**
     * @brief Clear the current file
     */
    void clear();

    /**
     * @brief Check if a file is currently loaded
     */
    [[nodiscard]] bool hasData() const { return _has_data; }

    /**
     * @brief Get the parsed file metadata
     */
    [[nodiscard]] NpyFileInfo const & fileInfo() const { return _file_info; }

    /**
     * @brief Get the total number of elements in the array
     */
    [[nodiscard]] qint64 totalElements() const { return _file_info.total_elements; }

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
     * @brief Emitted when file loading fails
     * @param message Error message
     */
    void loadError(QString const & message);

    /**
     * @brief Emitted when file is successfully loaded
     * @param num_rows Number of rows in the view
     * @param num_cols Number of columns in the view
     */
    void fileLoaded(qint64 num_rows, int num_cols);

private:
    /**
     * @brief Load a chunk of data starting at the given row
     * @param start_row First row to load
     * @return Pointer to loaded chunk, or nullptr on failure
     */
    NpyPreviewDataChunk * loadChunk(qint64 start_row) const;

    /**
     * @brief Get the chunk index for a given row
     */
    [[nodiscard]] qint64 chunkIndexForRow(qint64 row) const;

    /**
     * @brief Read data from the .npy file for the specified row range
     * @param start_row First row to read
     * @param num_rows Number of rows to read
     * @return Vector of row data, or empty on failure
     */
    std::vector<std::vector<QVariant>> readNpyData(qint64 start_row, qint64 num_rows) const;

    // File info
    QString _file_path;
    bool _has_data = false;
    NpyFileInfo _file_info;

    // Dimensions for display
    qint64 _num_rows = 0;
    int _num_cols = 0;

    // Chunk loading configuration
    int _chunk_size = 100;

    // Cache for loaded chunks (mutable for lazy loading in const methods)
    mutable QCache<qint64, NpyPreviewDataChunk> _chunk_cache;
};

#endif // NPY_DATASET_PREVIEW_MODEL_HPP
