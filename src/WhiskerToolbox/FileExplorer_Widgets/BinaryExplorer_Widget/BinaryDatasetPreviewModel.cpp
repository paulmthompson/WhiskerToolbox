#include "BinaryDatasetPreviewModel.hpp"

#include <QFile>

#include <algorithm>
#include <bit>
#include <cstring>
#include <fstream>

unsigned int binaryDataTypeSize(BinaryDataType dtype) {
    switch (dtype) {
        case BinaryDataType::Int8:    return 1;
        case BinaryDataType::Int16:   return 2;
        case BinaryDataType::Int32:   return 4;
        case BinaryDataType::Int64:   return 8;
        case BinaryDataType::UInt8:   return 1;
        case BinaryDataType::UInt16:  return 2;
        case BinaryDataType::UInt32:  return 4;
        case BinaryDataType::UInt64:  return 8;
        case BinaryDataType::Float32: return 4;
        case BinaryDataType::Float64: return 8;
    }
    return 0;
}

QString binaryDataTypeName(BinaryDataType dtype) {
    switch (dtype) {
        case BinaryDataType::Int8:    return QStringLiteral("int8");
        case BinaryDataType::Int16:   return QStringLiteral("int16");
        case BinaryDataType::Int32:   return QStringLiteral("int32");
        case BinaryDataType::Int64:   return QStringLiteral("int64");
        case BinaryDataType::UInt8:   return QStringLiteral("uint8");
        case BinaryDataType::UInt16:  return QStringLiteral("uint16");
        case BinaryDataType::UInt32:  return QStringLiteral("uint32");
        case BinaryDataType::UInt64:  return QStringLiteral("uint64");
        case BinaryDataType::Float32: return QStringLiteral("float32");
        case BinaryDataType::Float64: return QStringLiteral("float64");
    }
    return QStringLiteral("unknown");
}

int binaryDataTypeBits(BinaryDataType dtype) {
    switch (dtype) {
        case BinaryDataType::Int8:
        case BinaryDataType::UInt8:   return 8;
        case BinaryDataType::Int16:
        case BinaryDataType::UInt16:  return 16;
        case BinaryDataType::Int32:
        case BinaryDataType::UInt32:  return 32;
        case BinaryDataType::Int64:
        case BinaryDataType::UInt64:  return 64;
        case BinaryDataType::Float32:
        case BinaryDataType::Float64: return 0; // bitwise expansion not meaningful for floats
    }
    return 0;
}

// ----------------------------------------------------------------------------
// BinaryDatasetPreviewModel
// ----------------------------------------------------------------------------

BinaryDatasetPreviewModel::BinaryDatasetPreviewModel(QObject * parent)
    : QAbstractTableModel(parent)
    , _chunk_cache(20)
{
}

BinaryDatasetPreviewModel::~BinaryDatasetPreviewModel() = default;

bool BinaryDatasetPreviewModel::loadFile(QString const & file_path,
                                          BinaryParseConfig const & config) {
    beginResetModel();
    clear();

    QFile file(file_path);
    if (!file.exists()) {
        emit loadError(tr("File not found: %1").arg(file_path));
        endResetModel();
        return false;
    }

    _file_info.file_size = file.size();
    _config = config;

    if (_config.header_size < 0) {
        _config.header_size = 0;
    }
    if (_config.num_channels < 1) {
        _config.num_channels = 1;
    }

    _file_info.item_size = binaryDataTypeSize(_config.data_type);
    if (_file_info.item_size == 0) {
        emit loadError(tr("Invalid data type"));
        endResetModel();
        return false;
    }

    _file_info.data_size = _file_info.file_size - _config.header_size;
    if (_file_info.data_size <= 0) {
        emit loadError(tr("Header size (%1) is >= file size (%2)")
                       .arg(_config.header_size)
                       .arg(_file_info.file_size));
        endResetModel();
        return false;
    }

    _file_info.total_elements = _file_info.data_size / _file_info.item_size;
    _file_info.remainder_bytes = _file_info.data_size % _file_info.item_size;
    _file_info.total_rows = _file_info.total_elements / _config.num_channels;

    if (_config.bitwise_expand) {
        int bits = binaryDataTypeBits(_config.data_type);
        if (bits == 0) {
            // Fall back to normal mode for float types
            _config.bitwise_expand = false;
            _file_info.display_columns = _config.num_channels;
        } else {
            _file_info.display_columns = _config.num_channels * bits;
        }
    } else {
        _file_info.display_columns = _config.num_channels;
    }

    _num_rows = _file_info.total_rows;
    _num_cols = _file_info.display_columns;

    _file_path = file_path;
    _has_data = true;

    endResetModel();
    emit fileLoaded(_num_rows, _num_cols);
    return true;
}

bool BinaryDatasetPreviewModel::reloadWithConfig(BinaryParseConfig const & config) {
    if (_file_path.isEmpty()) {
        return false;
    }
    return loadFile(_file_path, config);
}

void BinaryDatasetPreviewModel::clear() {
    _file_path.clear();
    _has_data = false;
    _config = BinaryParseConfig{};
    _file_info = BinaryFileInfo{};
    _num_rows = 0;
    _num_cols = 0;
    _chunk_cache.clear();
}

void BinaryDatasetPreviewModel::setChunkSize(int size) {
    _chunk_size = std::max(10, size);
    _chunk_cache.clear();
}

int BinaryDatasetPreviewModel::rowCount(QModelIndex const & parent) const {
    if (parent.isValid()) {
        return 0;
    }
    constexpr qint64 max_display_rows = 1000000;
    return static_cast<int>(std::min(_num_rows, max_display_rows));
}

int BinaryDatasetPreviewModel::columnCount(QModelIndex const & parent) const {
    if (parent.isValid()) {
        return 0;
    }
    constexpr int max_display_cols = 256;
    return std::min(_num_cols, max_display_cols);
}

QVariant BinaryDatasetPreviewModel::data(QModelIndex const & index, int role) const {
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
    BinaryPreviewDataChunk * chunk = _chunk_cache.object(chunk_idx);

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

QVariant BinaryDatasetPreviewModel::headerData(int section, Qt::Orientation orientation,
                                                int role) const {
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    if (orientation == Qt::Horizontal) {
        if (_config.bitwise_expand) {
            int bits_per_element = binaryDataTypeBits(_config.data_type);
            if (bits_per_element > 0 && _config.num_channels > 1) {
                int channel = section / bits_per_element;
                int bit = section % bits_per_element;
                return QString("Ch%1.b%2").arg(channel).arg(bit);
            } else if (bits_per_element > 0) {
                return QString("bit%1").arg(section);
            }
        }
        if (_num_cols == 1) {
            return tr("Value");
        }
        return QString("Ch%1").arg(section);
    } else {
        return QString::number(section);
    }
}

qint64 BinaryDatasetPreviewModel::chunkIndexForRow(qint64 row) const {
    return row / _chunk_size;
}

BinaryPreviewDataChunk * BinaryDatasetPreviewModel::loadChunk(qint64 start_row) const {
    qint64 rows_to_load = std::min(static_cast<qint64>(_chunk_size), _num_rows - start_row);

    if (rows_to_load <= 0) {
        return nullptr;
    }

    auto data = readBinaryData(start_row, rows_to_load);
    if (data.empty()) {
        return nullptr;
    }

    auto * chunk = new BinaryPreviewDataChunk();
    chunk->start_row = start_row;
    chunk->rows = std::move(data);

    return chunk;
}

void BinaryDatasetPreviewModel::byteSwapIfNeeded(char * ptr, unsigned int size,
                                                   BinaryByteOrder order) {
    // Determine native endianness at compile time
    constexpr bool native_little = (std::endian::native == std::endian::little);
    bool need_swap = (order == BinaryByteOrder::BigEndian && native_little) ||
                     (order == BinaryByteOrder::LittleEndian && !native_little);

    if (!need_swap || size <= 1) {
        return;
    }

    for (unsigned int i = 0; i < size / 2; ++i) {
        std::swap(ptr[i], ptr[size - 1 - i]);
    }
}

std::vector<QVariant> BinaryDatasetPreviewModel::expandBits(char const * ptr) const {
    int bits = binaryDataTypeBits(_config.data_type);
    unsigned int item_size = _file_info.item_size;
    std::vector<QVariant> result;
    result.reserve(static_cast<size_t>(bits));

    // Read the raw value into a uint64_t for uniform bit access
    uint64_t val = 0;
    std::memcpy(&val, ptr, std::min(item_size, static_cast<unsigned int>(sizeof(uint64_t))));

    for (int b = 0; b < bits; ++b) {
        int bit_val = (val >> b) & 1;
        result.emplace_back(bit_val);
    }

    return result;
}

std::vector<std::vector<QVariant>> BinaryDatasetPreviewModel::readBinaryData(
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

        auto const bytes_per_element = static_cast<qint64>(_file_info.item_size);
        qint64 elements_per_row = _config.num_channels;
        qint64 row_size_bytes = elements_per_row * bytes_per_element;
        qint64 seek_pos = _config.header_size + start_row * row_size_bytes;

        stream.seekg(seek_pos);
        if (!stream) {
            return result;
        }

        qint64 total_bytes = num_rows * row_size_bytes;
        std::vector<char> buffer(static_cast<size_t>(total_bytes));
        stream.read(buffer.data(), total_bytes);
        if (!stream) {
            auto bytes_read = stream.gcount();
            if (bytes_read <= 0) {
                return result;
            }
            num_rows = bytes_read / row_size_bytes;
            if (num_rows <= 0) {
                return result;
            }
        }

        result.reserve(static_cast<size_t>(num_rows));

        BinaryDataType dtype = _config.data_type;
        unsigned int item_size = _file_info.item_size;
        bool do_bitwise = _config.bitwise_expand && binaryDataTypeBits(dtype) > 0;

        for (qint64 r = 0; r < num_rows; ++r) {
            std::vector<QVariant> row;
            row.reserve(static_cast<size_t>(_num_cols));

            for (int c = 0; c < _config.num_channels; ++c) {
                auto offset = static_cast<size_t>(
                    (r * elements_per_row + c) * bytes_per_element);

                if (offset + item_size > buffer.size()) {
                    // Fill remaining columns with placeholder
                    for (int i = 0; i < (do_bitwise ? binaryDataTypeBits(dtype) : 1); ++i) {
                        row.emplace_back(QVariant("?"));
                    }
                    continue;
                }

                char * ptr = buffer.data() + offset;

                // Handle byte swapping for multi-byte types
                // We work on a copy to avoid modifying the buffer
                char element_buf[8];
                std::memcpy(element_buf, ptr, item_size);
                byteSwapIfNeeded(element_buf, item_size, _config.byte_order);

                if (do_bitwise) {
                    auto bits = expandBits(element_buf);
                    for (auto & bv : bits) {
                        row.push_back(std::move(bv));
                    }
                } else {
                    switch (dtype) {
                        case BinaryDataType::Int8: {
                            int8_t val = 0;
                            std::memcpy(&val, element_buf, sizeof(int8_t));
                            row.emplace_back(static_cast<int>(val));
                            break;
                        }
                        case BinaryDataType::Int16: {
                            int16_t val = 0;
                            std::memcpy(&val, element_buf, sizeof(int16_t));
                            row.emplace_back(static_cast<int>(val));
                            break;
                        }
                        case BinaryDataType::Int32: {
                            int32_t val = 0;
                            std::memcpy(&val, element_buf, sizeof(int32_t));
                            row.emplace_back(val);
                            break;
                        }
                        case BinaryDataType::Int64: {
                            int64_t val = 0;
                            std::memcpy(&val, element_buf, sizeof(int64_t));
                            row.emplace_back(static_cast<qlonglong>(val));
                            break;
                        }
                        case BinaryDataType::UInt8: {
                            uint8_t val = 0;
                            std::memcpy(&val, element_buf, sizeof(uint8_t));
                            row.emplace_back(static_cast<int>(val));
                            break;
                        }
                        case BinaryDataType::UInt16: {
                            uint16_t val = 0;
                            std::memcpy(&val, element_buf, sizeof(uint16_t));
                            row.emplace_back(static_cast<int>(val));
                            break;
                        }
                        case BinaryDataType::UInt32: {
                            uint32_t val = 0;
                            std::memcpy(&val, element_buf, sizeof(uint32_t));
                            row.emplace_back(val);
                            break;
                        }
                        case BinaryDataType::UInt64: {
                            uint64_t val = 0;
                            std::memcpy(&val, element_buf, sizeof(uint64_t));
                            row.emplace_back(static_cast<qulonglong>(val));
                            break;
                        }
                        case BinaryDataType::Float32: {
                            float val = 0;
                            std::memcpy(&val, element_buf, sizeof(float));
                            row.emplace_back(static_cast<double>(val));
                            break;
                        }
                        case BinaryDataType::Float64: {
                            double val = 0;
                            std::memcpy(&val, element_buf, sizeof(double));
                            row.emplace_back(val);
                            break;
                        }
                    }
                }
            }
            result.push_back(std::move(row));
        }

    } catch (std::exception const &) {
        result.clear();
    }

    return result;
}
