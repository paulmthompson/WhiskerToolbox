#include "CsvDatasetPreviewModel.hpp"

#include <QFile>
#include <QTextStream>

#include <algorithm>
#include <fstream>

// ----------------------------------------------------------------------------
// CsvDatasetPreviewModel
// ----------------------------------------------------------------------------

CsvDatasetPreviewModel::CsvDatasetPreviewModel(QObject * parent)
    : QAbstractTableModel(parent)
    , _chunk_cache(20)
{
}

CsvDatasetPreviewModel::~CsvDatasetPreviewModel() = default;

bool CsvDatasetPreviewModel::loadFile(QString const & file_path,
                                       CsvParseConfig const & config) {
    beginResetModel();
    clear();

    QFile file(file_path);
    if (!file.exists()) {
        emit loadError(tr("File not found: %1").arg(file_path));
        endResetModel();
        return false;
    }

    _file_info.file_size = file.size();
    if (_file_info.file_size == 0) {
        emit loadError(tr("File is empty: %1").arg(file_path));
        endResetModel();
        return false;
    }

    _config = config;
    _file_path = file_path;

    // Validate config
    if (_config.column_delimiter.isEmpty()) {
        _config.column_delimiter = ",";
    }
    if (_config.header_lines < 0) {
        _config.header_lines = 0;
    }

    // Build the line offset index
    if (!buildLineIndex()) {
        emit loadError(tr("Failed to scan file: %1").arg(file_path));
        endResetModel();
        return false;
    }

    _file_info.total_lines = static_cast<qint64>(_line_offsets.size());

    if (_file_info.total_lines <= _config.header_lines) {
        emit loadError(tr("File has %1 lines but %2 header lines configured")
                       .arg(_file_info.total_lines)
                       .arg(_config.header_lines));
        endResetModel();
        return false;
    }

    _file_info.data_rows = _file_info.total_lines - _config.header_lines;

    // Read column names from first header line if configured
    if (_config.header_lines > 0 && _config.use_first_header_as_names) {
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            emit loadError(tr("Cannot open file: %1").arg(file_path));
            endResetModel();
            return false;
        }

        // Seek to the first line
        file.seek(_line_offsets[0]);
        QTextStream stream(&file);
        QString header_line = stream.readLine();
        file.close();

        _file_info.column_names = splitLine(header_line);

        if (_config.trim_whitespace) {
            for (auto & name : _file_info.column_names) {
                name = name.trimmed();
            }
        }
    }

    // Detect number of columns from first data line
    {
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            emit loadError(tr("Cannot open file: %1").arg(file_path));
            endResetModel();
            return false;
        }

        auto first_data_idx = static_cast<size_t>(_config.header_lines);
        file.seek(_line_offsets[first_data_idx]);
        QTextStream stream(&file);
        QString first_data_line = stream.readLine();
        file.close();

        auto fields = splitLine(first_data_line);
        _file_info.detected_columns = static_cast<int>(fields.size());
    }

    // Generate column names if we don't have them from the header
    if (_file_info.column_names.isEmpty()) {
        for (int i = 0; i < _file_info.detected_columns; ++i) {
            _file_info.column_names.append(QString("Col%1").arg(i));
        }
    }

    // Use the max of header column count and detected data column count
    _num_cols = static_cast<int>(std::max(_file_info.column_names.size(),
                         static_cast<qsizetype>(_file_info.detected_columns)));
    _num_rows = _file_info.data_rows;

    // Pad column names if needed
    while (_file_info.column_names.size() < _num_cols) {
        _file_info.column_names.append(
            QString("Col%1").arg(_file_info.column_names.size()));
    }

    _has_data = true;

    endResetModel();
    emit fileLoaded(_num_rows, static_cast<int>(_num_cols));
    return true;
}

bool CsvDatasetPreviewModel::reloadWithConfig(CsvParseConfig const & config) {
    if (_file_path.isEmpty()) {
        return false;
    }
    // Copy path since loadFile calls clear() which resets _file_path
    QString path = _file_path;
    return loadFile(path, config);
}

void CsvDatasetPreviewModel::clear() {
    _file_path.clear();
    _has_data = false;
    _config = CsvParseConfig{};
    _file_info = CsvFileInfo{};
    _line_offsets.clear();
    _num_rows = 0;
    _num_cols = 0;
    _chunk_cache.clear();
}

void CsvDatasetPreviewModel::setChunkSize(int size) {
    _chunk_size = std::max(10, size);
    _chunk_cache.clear();
}

int CsvDatasetPreviewModel::rowCount(QModelIndex const & parent) const {
    if (parent.isValid()) {
        return 0;
    }
    constexpr qint64 max_display_rows = 1000000;
    return static_cast<int>(std::min(_num_rows, max_display_rows));
}

int CsvDatasetPreviewModel::columnCount(QModelIndex const & parent) const {
    if (parent.isValid()) {
        return 0;
    }
    constexpr int max_display_cols = 256;
    return std::min(static_cast<int>(_num_cols), max_display_cols);
}

QVariant CsvDatasetPreviewModel::data(QModelIndex const & index, int role) const {
    if (!index.isValid() || !_has_data) {
        return QVariant();
    }

    if (role != Qt::DisplayRole && role != Qt::ToolTipRole) {
        return QVariant();
    }

    qint64 row = index.row();
    int col = index.column();

    if (row < 0 || row >= _num_rows || col < 0 || col >= _num_cols) {
        return QVariant();
    }

    qint64 chunk_idx = chunkIndexForRow(row);
    CsvPreviewDataChunk * chunk = _chunk_cache.object(chunk_idx);

    if (!chunk) {
        chunk = loadChunk(chunk_idx * _chunk_size);
        if (!chunk) {
            return QVariant("Error");
        }
        _chunk_cache.insert(chunk_idx, chunk);
    }

    qint64 row_in_chunk = row - chunk->start_row;
    if (row_in_chunk < 0 || row_in_chunk >= static_cast<qint64>(chunk->rows.size())) {
        return QVariant("?");
    }

    auto const & row_data = chunk->rows[static_cast<size_t>(row_in_chunk)];
    if (col >= static_cast<int>(row_data.size())) {
        return QVariant();
    }

    return row_data[static_cast<size_t>(col)];
}

QVariant CsvDatasetPreviewModel::headerData(int section, Qt::Orientation orientation,
                                             int role) const {
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    if (orientation == Qt::Horizontal) {
        if (section < _file_info.column_names.size()) {
            return _file_info.column_names[section];
        }
        return QString("Col%1").arg(section);
    } else {
        // Row header: show the actual data row number (0-based)
        return QString::number(section);
    }
}

qint64 CsvDatasetPreviewModel::chunkIndexForRow(qint64 row) const {
    return row / _chunk_size;
}

CsvPreviewDataChunk * CsvDatasetPreviewModel::loadChunk(qint64 start_row) const {
    qint64 rows_to_load = std::min(static_cast<qint64>(_chunk_size), _num_rows - start_row);

    if (rows_to_load <= 0) {
        return nullptr;
    }

    auto data = readCsvData(start_row, rows_to_load);
    if (data.empty()) {
        return nullptr;
    }

    auto * chunk = new CsvPreviewDataChunk();
    chunk->start_row = start_row;
    chunk->rows = std::move(data);

    return chunk;
}

bool CsvDatasetPreviewModel::buildLineIndex() {
    _line_offsets.clear();

    QFile file(_file_path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    // Scan file for line boundaries
    // We store the byte offset of the start of each line
    constexpr qint64 buffer_size = 65536;
    QByteArray buffer;
    qint64 file_pos = 0;
    bool at_line_start = true;

    while (!file.atEnd()) {
        buffer = file.read(buffer_size);
        if (buffer.isEmpty()) {
            break;
        }

        for (int i = 0; i < buffer.size(); ++i) {
            if (at_line_start) {
                _line_offsets.push_back(file_pos + i);
                at_line_start = false;
            }

            char c = buffer[i];
            if (c == '\n') {
                at_line_start = true;
            } else if (c == '\r') {
                // Handle \r\n: peek at next char
                if (i + 1 < buffer.size()) {
                    if (buffer[i + 1] == '\n') {
                        ++i; // skip the \n
                    }
                } else {
                    // \r at the end of the buffer; peek from the file
                    qint64 peek_pos = file.pos();
                    char next = 0;
                    if (file.read(&next, 1) == 1) {
                        if (next != '\n') {
                            file.seek(peek_pos); // put it back
                        }
                    }
                }
                at_line_start = true;
            }
        }
        file_pos += buffer.size();
    }

    file.close();

    // Remove any empty trailing line if the file ends with a newline
    // This avoids counting a phantom empty line at the end
    if (!_line_offsets.empty()) {
        qint64 last_offset = _line_offsets.back();
        if (last_offset >= _file_info.file_size) {
            _line_offsets.pop_back();
        }
    }

    return !_line_offsets.empty();
}

QStringList CsvDatasetPreviewModel::splitLine(QString const & line) const {
    QStringList fields;

    if (_config.column_delimiter.isEmpty()) {
        fields.append(line);
        return fields;
    }

    QString quote = _config.quote_char;
    bool has_quote = !quote.isEmpty();
    QChar quote_char = has_quote ? quote[0] : QChar();

    QString current_field;
    bool in_quotes = false;
    qsizetype i = 0;
    auto len = line.length();
    auto delim_len = _config.column_delimiter.length();

    while (i < len) {
        if (in_quotes) {
            if (has_quote && line[i] == quote_char) {
                // Check for escaped quote (doubled quote char)
                if (i + 1 < len && line[i + 1] == quote_char) {
                    current_field += quote_char;
                    i += 2;
                } else {
                    // End of quoted section
                    in_quotes = false;
                    ++i;
                }
            } else {
                current_field += line[i];
                ++i;
            }
        } else {
            if (has_quote && line[i] == quote_char) {
                in_quotes = true;
                ++i;
            } else if (line.mid(i, delim_len) == _config.column_delimiter) {
                if (_config.trim_whitespace) {
                    fields.append(current_field.trimmed());
                } else {
                    fields.append(current_field);
                }
                current_field.clear();
                i += delim_len;
            } else {
                current_field += line[i];
                ++i;
            }
        }
    }

    // Add last field
    if (_config.trim_whitespace) {
        fields.append(current_field.trimmed());
    } else {
        fields.append(current_field);
    }

    return fields;
}

std::vector<std::vector<QVariant>> CsvDatasetPreviewModel::readCsvData(
    qint64 start_row, qint64 num_rows) const {

    std::vector<std::vector<QVariant>> result;

    if (!_has_data || num_rows <= 0) {
        return result;
    }

    try {
        QFile file(_file_path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return result;
        }

        // Convert data row index to file line index (skip header lines)
        qint64 file_line_start = start_row + _config.header_lines;

        if (file_line_start < 0 ||
            static_cast<size_t>(file_line_start) >= _line_offsets.size()) {
            return result;
        }

        // Seek to the start line
        file.seek(_line_offsets[static_cast<size_t>(file_line_start)]);
        QTextStream stream(&file);

        result.reserve(static_cast<size_t>(num_rows));

        for (qint64 r = 0; r < num_rows; ++r) {
            if (stream.atEnd()) {
                break;
            }

            QString line = stream.readLine();
            if (line.isNull()) {
                break;
            }

            QStringList fields = splitLine(line);

            std::vector<QVariant> row;
            row.reserve(static_cast<size_t>(_num_cols));

            for (int c = 0; c < _num_cols; ++c) {
                if (c < fields.size()) {
                    row.emplace_back(fields[c]);
                } else {
                    row.emplace_back(QVariant());
                }
            }

            result.push_back(std::move(row));
        }

        file.close();
    } catch (std::exception const &) {
        result.clear();
    }

    return result;
}
