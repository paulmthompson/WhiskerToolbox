#ifndef CSV_DATASET_PREVIEW_MODEL_HPP
#define CSV_DATASET_PREVIEW_MODEL_HPP

/**
 * @file CsvDatasetPreviewModel.hpp
 * @brief Lazy-loading table model for previewing CSV/delimited text file contents
 *
 * This model only loads data chunks as they become visible in the view,
 * making it efficient for large files. It parses the file according to
 * user-specified delimiters and header configuration, then reads only
 * the requested portion of data.
 */

#include <QAbstractTableModel>
#include <QCache>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <cstdint>
#include <vector>

/**
 * @brief Configuration for how to parse a CSV file
 */
struct CsvParseConfig {
    QString column_delimiter = ",";    ///< Column delimiter string (e.g. ",", "\t", ";", "|")
    QString line_delimiter = "\n";     ///< Line delimiter ("\n", "\r\n", "\r")
    int header_lines = 1;             ///< Number of header lines to skip (0 = no header)
    bool use_first_header_as_names = true; ///< Use the first header line as column names
    QString quote_char = "\"";         ///< Character used for quoting fields
    bool trim_whitespace = true;       ///< Trim leading/trailing whitespace from fields
};

/**
 * @brief Cached chunk of row data for the preview model
 */
struct CsvPreviewDataChunk {
    qint64 start_row;
    std::vector<std::vector<QVariant>> rows;
};

/**
 * @brief Metadata computed from parsing a CSV file with a given config
 */
struct CsvFileInfo {
    qint64 file_size = 0;             ///< Total file size in bytes
    qint64 total_lines = 0;           ///< Total number of lines in the file
    qint64 data_rows = 0;             ///< Number of data rows (total_lines - header_lines)
    int detected_columns = 0;         ///< Number of columns detected from first data line
    QStringList column_names;          ///< Column names (from header or generated)
    QString detected_encoding;         ///< Detected text encoding
};

/**
 * @brief Lazy-loading table model for CSV file preview
 *
 * This model presents CSV data in a table format:
 * - Rows correspond to data lines (after skipping header lines)
 * - Columns are determined by splitting the first data line on the delimiter
 * - Column headers come from the first header line (if configured) or are auto-generated
 *
 * Data is loaded in chunks (default 100 rows) and cached using QCache
 * to provide smooth scrolling while minimizing memory usage.
 *
 * The model supports line-based random access by pre-scanning the file
 * to build a line offset index on load.
 */
class CsvDatasetPreviewModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit CsvDatasetPreviewModel(QObject * parent = nullptr);
    ~CsvDatasetPreviewModel() override;

    /**
     * @brief Load a CSV file for preview with the given parse configuration
     *
     * @param file_path Path to the CSV file
     * @param config Parse configuration
     * @return true if file was loaded successfully
     */
    bool loadFile(QString const & file_path, CsvParseConfig const & config);

    /**
     * @brief Reload the current file with a new parse configuration
     *
     * @param config New parse configuration
     * @return true if reload was successful
     */
    bool reloadWithConfig(CsvParseConfig const & config);

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
    [[nodiscard]] CsvFileInfo const & fileInfo() const { return _file_info; }

    /**
     * @brief Get the current parse configuration
     */
    [[nodiscard]] CsvParseConfig const & parseConfig() const { return _config; }

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
    CsvPreviewDataChunk * loadChunk(qint64 start_row) const;

    /**
     * @brief Get the chunk index for a given row
     */
    [[nodiscard]] qint64 chunkIndexForRow(qint64 row) const;

    /**
     * @brief Read CSV data for the specified row range
     * @param start_row First data row to read (0-based, after header)
     * @param num_rows Number of rows to read
     * @return Vector of row data, or empty on failure
     */
    std::vector<std::vector<QVariant>> readCsvData(qint64 start_row, qint64 num_rows) const;

    /**
     * @brief Build a line offset index by scanning the file
     *
     * Stores the byte offset of each line start in _line_offsets.
     * @return true on success
     */
    bool buildLineIndex();

    /**
     * @brief Split a line into fields using the configured delimiter and quote char
     * @param line The text line to split
     * @return List of field values
     */
    [[nodiscard]] QStringList splitLine(QString const & line) const;

    // File state
    QString _file_path;
    bool _has_data = false;
    CsvParseConfig _config;
    CsvFileInfo _file_info;

    // Line offset index for random access (byte offset of each line start)
    std::vector<qint64> _line_offsets;

    // Dimensions for display
    qint64 _num_rows = 0;
    int _num_cols = 0;

    // Chunk loading configuration
    int _chunk_size = 100;

    // Cache for loaded chunks (mutable for lazy loading in const methods)
    mutable QCache<qint64, CsvPreviewDataChunk> _chunk_cache;
};

#endif // CSV_DATASET_PREVIEW_MODEL_HPP
