#include "NpyDatasetPreviewModel.hpp"

#include "npy.hpp"

#include <QFile>

#include <algorithm>
#include <cstring>
#include <fstream>

NpyDatasetPreviewModel::NpyDatasetPreviewModel(QObject * parent)
    : QAbstractTableModel(parent)
    , _chunk_cache(20)  // Cache up to 20 chunks (2000 rows with default chunk size)
{
}

NpyDatasetPreviewModel::~NpyDatasetPreviewModel() = default;

bool NpyDatasetPreviewModel::loadFile(QString const & file_path) {
    beginResetModel();
    clear();

    try {
        std::ifstream stream(file_path.toStdString(), std::ifstream::binary);
        if (!stream) {
            emit loadError(tr("Failed to open file: %1").arg(file_path));
            endResetModel();
            return false;
        }

        // Read and parse the .npy header using libnpy
        std::string header_s = npy::read_header(stream);
        npy::header_t header = npy::parse_header(header_s);

        // Record the data offset (position after header)
        _file_info.data_offset = static_cast<qint64>(stream.tellg());

        // Parse dtype info
        _file_info.byte_order = header.dtype.byteorder;
        _file_info.type_kind = header.dtype.kind;
        _file_info.item_size = header.dtype.itemsize;
        _file_info.dtype_str = QString::fromStdString(header.dtype.str());
        _file_info.fortran_order = header.fortran_order;

        // Parse shape
        _file_info.shape.clear();
        _file_info.total_elements = 1;
        for (auto dim : header.shape) {
            _file_info.shape.push_back(static_cast<uint64_t>(dim));
            _file_info.total_elements *= static_cast<qint64>(dim);
        }

        // Determine table layout based on dimensionality
        auto ndims = static_cast<int>(_file_info.shape.size());
        if (ndims == 0) {
            // Scalar
            _num_rows = 1;
            _num_cols = 1;
        } else if (ndims == 1) {
            _num_rows = static_cast<qint64>(_file_info.shape[0]);
            _num_cols = 1;
        } else if (ndims == 2) {
            _num_rows = static_cast<qint64>(_file_info.shape[0]);
            _num_cols = static_cast<int>(_file_info.shape[1]);
        } else {
            // ND: first dim is rows, rest flattened
            _num_rows = static_cast<qint64>(_file_info.shape[0]);
            _num_cols = 1;
            for (size_t i = 1; i < _file_info.shape.size(); ++i) {
                _num_cols *= static_cast<int>(_file_info.shape[i]);
            }
        }

        _file_path = file_path;
        _has_data = true;

    } catch (std::exception const & e) {
        QString msg = tr("Failed to parse .npy file: %1").arg(QString::fromUtf8(e.what()));
        emit loadError(msg);
        endResetModel();
        return false;
    }

    endResetModel();
    emit fileLoaded(_num_rows, _num_cols);
    return true;
}

void NpyDatasetPreviewModel::clear() {
    _file_path.clear();
    _has_data = false;
    _file_info = NpyFileInfo{};
    _num_rows = 0;
    _num_cols = 0;
    _chunk_cache.clear();
}

void NpyDatasetPreviewModel::setChunkSize(int size) {
    _chunk_size = std::max(10, size);
    _chunk_cache.clear();
}

int NpyDatasetPreviewModel::rowCount(QModelIndex const & parent) const {
    if (parent.isValid()) {
        return 0;
    }
    constexpr qint64 max_display_rows = 1000000;
    return static_cast<int>(std::min(_num_rows, max_display_rows));
}

int NpyDatasetPreviewModel::columnCount(QModelIndex const & parent) const {
    if (parent.isValid()) {
        return 0;
    }
    constexpr int max_display_cols = 100;
    return std::min(_num_cols, max_display_cols);
}

QVariant NpyDatasetPreviewModel::data(QModelIndex const & index, int role) const {
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

    // Find or load the chunk containing this row
    qint64 chunk_idx = chunkIndexForRow(row);
    NpyPreviewDataChunk * chunk = _chunk_cache.object(chunk_idx);

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
        return QVariant("?");
    }

    return row_data[static_cast<size_t>(col)];
}

QVariant NpyDatasetPreviewModel::headerData(int section, Qt::Orientation orientation,
                                             int role) const {
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    if (orientation == Qt::Horizontal) {
        if (_num_cols == 1) {
            return tr("Value");
        }
        return QString("[%1]").arg(section);
    } else {
        return QString::number(section);
    }
}

qint64 NpyDatasetPreviewModel::chunkIndexForRow(qint64 row) const {
    return row / _chunk_size;
}

NpyPreviewDataChunk * NpyDatasetPreviewModel::loadChunk(qint64 start_row) const {
    qint64 rows_to_load = std::min(static_cast<qint64>(_chunk_size), _num_rows - start_row);

    if (rows_to_load <= 0) {
        return nullptr;
    }

    auto data = readNpyData(start_row, rows_to_load);
    if (data.empty()) {
        return nullptr;
    }

    auto * chunk = new NpyPreviewDataChunk();
    chunk->start_row = start_row;
    chunk->rows = std::move(data);

    return chunk;
}

std::vector<std::vector<QVariant>> NpyDatasetPreviewModel::readNpyData(
    qint64 start_row, qint64 num_rows) const {

    std::vector<std::vector<QVariant>> result;

    if (!_has_data || num_rows <= 0) {
        return result;
    }

    try {
        std::ifstream stream(_file_path.toStdString(), std::ifstream::binary);
        if (!stream) {
            return result;
        }

        // Seek to the data offset + row offset
        auto const bytes_per_element = static_cast<qint64>(_file_info.item_size);
        qint64 elements_per_row = _num_cols;
        qint64 row_size_bytes = elements_per_row * bytes_per_element;
        qint64 seek_pos = _file_info.data_offset + start_row * row_size_bytes;

        stream.seekg(seek_pos);
        if (!stream) {
            return result;
        }

        qint64 total_elements_to_read = num_rows * elements_per_row;
        qint64 total_bytes = total_elements_to_read * bytes_per_element;

        // Read raw bytes
        std::vector<char> buffer(static_cast<size_t>(total_bytes));
        stream.read(buffer.data(), total_bytes);
        if (!stream) {
            // Partial read is okay for the last chunk
            auto bytes_read = stream.gcount();
            if (bytes_read <= 0) {
                return result;
            }
            // Adjust num_rows based on what we actually read
            num_rows = bytes_read / row_size_bytes;
            if (num_rows <= 0) {
                return result;
            }
        }

        result.reserve(static_cast<size_t>(num_rows));

        // Convert raw bytes to QVariants based on dtype
        char kind = _file_info.type_kind;
        unsigned int item_size = _file_info.item_size;

        for (qint64 r = 0; r < num_rows; ++r) {
            std::vector<QVariant> row;
            row.reserve(static_cast<size_t>(_num_cols));

            for (int c = 0; c < _num_cols; ++c) {
                auto offset = static_cast<size_t>(
                    (r * elements_per_row + c) * bytes_per_element);

                if (offset + item_size > buffer.size()) {
                    row.emplace_back(QVariant("?"));
                    continue;
                }

                char const * ptr = buffer.data() + offset;

                if (kind == 'f') {
                    // Float
                    if (item_size == 4) {
                        float val = 0;
                        std::memcpy(&val, ptr, sizeof(float));
                        row.emplace_back(static_cast<double>(val));
                    } else if (item_size == 8) {
                        double val = 0;
                        std::memcpy(&val, ptr, sizeof(double));
                        row.emplace_back(val);
                    } else {
                        row.emplace_back(tr("<float%1>").arg(item_size * 8));
                    }
                } else if (kind == 'i') {
                    // Signed integer
                    if (item_size == 1) {
                        int8_t val = 0;
                        std::memcpy(&val, ptr, sizeof(int8_t));
                        row.emplace_back(static_cast<int>(val));
                    } else if (item_size == 2) {
                        int16_t val = 0;
                        std::memcpy(&val, ptr, sizeof(int16_t));
                        row.emplace_back(static_cast<int>(val));
                    } else if (item_size == 4) {
                        int32_t val = 0;
                        std::memcpy(&val, ptr, sizeof(int32_t));
                        row.emplace_back(val);
                    } else if (item_size == 8) {
                        int64_t val = 0;
                        std::memcpy(&val, ptr, sizeof(int64_t));
                        row.emplace_back(static_cast<qlonglong>(val));
                    } else {
                        row.emplace_back(tr("<int%1>").arg(item_size * 8));
                    }
                } else if (kind == 'u') {
                    // Unsigned integer
                    if (item_size == 1) {
                        uint8_t val = 0;
                        std::memcpy(&val, ptr, sizeof(uint8_t));
                        row.emplace_back(static_cast<int>(val));
                    } else if (item_size == 2) {
                        uint16_t val = 0;
                        std::memcpy(&val, ptr, sizeof(uint16_t));
                        row.emplace_back(static_cast<int>(val));
                    } else if (item_size == 4) {
                        uint32_t val = 0;
                        std::memcpy(&val, ptr, sizeof(uint32_t));
                        row.emplace_back(val);
                    } else if (item_size == 8) {
                        uint64_t val = 0;
                        std::memcpy(&val, ptr, sizeof(uint64_t));
                        row.emplace_back(static_cast<qulonglong>(val));
                    } else {
                        row.emplace_back(tr("<uint%1>").arg(item_size * 8));
                    }
                } else if (kind == 'b') {
                    // Boolean
                    uint8_t val = 0;
                    std::memcpy(&val, ptr, sizeof(uint8_t));
                    row.emplace_back(val != 0 ? "True" : "False");
                } else {
                    row.emplace_back(tr("<unsupported>"));
                }
            }
            result.push_back(std::move(row));
        }

    } catch (std::exception const &) {
        result.clear();
    }

    return result;
}
