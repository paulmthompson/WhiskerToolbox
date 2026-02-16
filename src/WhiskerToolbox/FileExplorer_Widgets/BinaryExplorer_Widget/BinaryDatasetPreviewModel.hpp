#ifndef BINARY_DATASET_PREVIEW_MODEL_HPP
#define BINARY_DATASET_PREVIEW_MODEL_HPP

/**
 * @file BinaryDatasetPreviewModel.hpp
 * @brief Lazy-loading table model for previewing raw binary file contents
 * 
 * This model reads raw binary files according to user-specified parameters:
 * header offset, data type, number of channels, byte order, and optional
 * bitwise expansion for digital event data.
 * 
 * Data is loaded in chunks as they become visible in the view, making it
 * efficient for large files. The user can change parsing parameters and
 * the preview updates to reflect the new interpretation.
 */

#include <QAbstractTableModel>
#include <QCache>
#include <QString>
#include <QVariant>

#include <cstdint>
#include <vector>

/**
 * @brief Supported binary data types for parsing
 */
enum class BinaryDataType {
    Int8,
    Int16,
    Int32,
    Int64,
    UInt8,
    UInt16,
    UInt32,
    UInt64,
    Float32,
    Float64
};

/**
 * @brief Byte order for multi-byte data types
 */
enum class BinaryByteOrder {
    LittleEndian,
    BigEndian
};

/**
 * @brief Configuration for how to parse a binary file
 */
struct BinaryParseConfig {
    qint64 header_size = 0;                              ///< Bytes to skip at start of file
    BinaryDataType data_type = BinaryDataType::Float32;  ///< Data type to interpret bytes as
    int num_channels = 1;                                ///< Number of interleaved channels (columns)
    BinaryByteOrder byte_order = BinaryByteOrder::LittleEndian; ///< Byte order
    bool bitwise_expand = false;                         ///< Expand each value into individual bits
};

/**
 * @brief Cached chunk of data for the preview model
 */
struct BinaryPreviewDataChunk {
    qint64 start_row;
    std::vector<std::vector<QVariant>> rows;
};

/**
 * @brief Metadata computed from parsing a binary file with a given config
 */
struct BinaryFileInfo {
    qint64 file_size = 0;           ///< Total file size in bytes
    qint64 data_size = 0;           ///< Data size (file_size - header_size)
    qint64 total_elements = 0;      ///< Number of elements (data_size / item_size)
    qint64 total_rows = 0;          ///< Number of rows (total_elements / num_channels)
    int display_columns = 0;        ///< Columns shown in table (channels, or channels*bits if bitwise)
    unsigned int item_size = 0;     ///< Bytes per element for the selected data type
    qint64 remainder_bytes = 0;     ///< Leftover bytes that don't fit an element
};

/**
 * @brief Get the size in bytes of a BinaryDataType
 */
[[nodiscard]] unsigned int binaryDataTypeSize(BinaryDataType dtype);

/**
 * @brief Get a human-readable name for a BinaryDataType
 */
[[nodiscard]] QString binaryDataTypeName(BinaryDataType dtype);

/**
 * @brief Get the number of bits for bitwise expansion of a data type
 * @return Number of bits, or 0 if bitwise expansion is not meaningful
 */
[[nodiscard]] int binaryDataTypeBits(BinaryDataType dtype);

/**
 * @brief Lazy-loading table model for raw binary file preview
 * 
 * This model presents binary file data in a table format based on user
 * configuration (data type, channels, header offset, byte order).
 * 
 * When bitwise expansion is enabled, each element is expanded into
 * individual bit columns, useful for interpreting digital event data
 * from multi-channel acquisition devices.
 * 
 * Data is loaded in chunks (default 100 rows) and cached using QCache
 * to provide smooth scrolling while minimizing memory usage.
 */
class BinaryDatasetPreviewModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit BinaryDatasetPreviewModel(QObject * parent = nullptr);
    ~BinaryDatasetPreviewModel() override;

    /**
     * @brief Load a binary file for preview with the given parse configuration
     * 
     * @param file_path Path to the binary file
     * @param config Parse configuration
     * @return true if file was loaded successfully
     */
    bool loadFile(QString const & file_path, BinaryParseConfig const & config);

    /**
     * @brief Reload the current file with a new parse configuration
     * 
     * @param config New parse configuration
     * @return true if reload was successful
     */
    bool reloadWithConfig(BinaryParseConfig const & config);

    /**
     * @brief Clear the current file
     */
    void clear();

    /**
     * @brief Check if a file is currently loaded
     */
    [[nodiscard]] bool hasData() const { return _has_data; }

    /**
     * @brief Get the computed file information
     */
    [[nodiscard]] BinaryFileInfo const & fileInfo() const { return _file_info; }

    /**
     * @brief Get the current parse configuration
     */
    [[nodiscard]] BinaryParseConfig const & parseConfig() const { return _config; }

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
    BinaryPreviewDataChunk * loadChunk(qint64 start_row) const;

    /**
     * @brief Get the chunk index for a given row
     */
    [[nodiscard]] qint64 chunkIndexForRow(qint64 row) const;

    /**
     * @brief Read raw binary data for the specified row range
     * @param start_row First row to read
     * @param num_rows Number of rows to read
     * @return Vector of row data, or empty on failure
     */
    std::vector<std::vector<QVariant>> readBinaryData(qint64 start_row, qint64 num_rows) const;

    /**
     * @brief Byte-swap a value in-place according to configured byte order
     * @param ptr Pointer to the data
     * @param size Size of the element in bytes
     */
    static void byteSwapIfNeeded(char * ptr, unsigned int size, BinaryByteOrder order);

    /**
     * @brief Expand a single integer value into individual bit QVariants
     * @param ptr Pointer to the raw data
     * @return Vector of QVariant (0 or 1) for each bit
     */
    std::vector<QVariant> expandBits(char const * ptr) const;

    // File state
    QString _file_path;
    bool _has_data = false;
    BinaryParseConfig _config;
    BinaryFileInfo _file_info;

    // Dimensions for display
    qint64 _num_rows = 0;
    int _num_cols = 0;

    // Chunk loading configuration
    int _chunk_size = 100;

    // Cache for loaded chunks (mutable for lazy loading in const methods)
    mutable QCache<qint64, BinaryPreviewDataChunk> _chunk_cache;
};

#endif // BINARY_DATASET_PREVIEW_MODEL_HPP
