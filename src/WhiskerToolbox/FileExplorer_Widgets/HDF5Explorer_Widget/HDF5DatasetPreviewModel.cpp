#include "HDF5DatasetPreviewModel.hpp"

#include <H5Cpp.h>

#include <algorithm>
#include <cstring>

HDF5DatasetPreviewModel::HDF5DatasetPreviewModel(QObject * parent)
    : QAbstractTableModel(parent)
    , _chunk_cache(20)  // Cache up to 20 chunks (2000 rows with default chunk size)
{
}

HDF5DatasetPreviewModel::~HDF5DatasetPreviewModel() = default;

bool HDF5DatasetPreviewModel::loadDataset(QString const & file_path, 
                                           QString const & dataset_path) {
    beginResetModel();
    clear();
    
    try {
        H5::H5File file(file_path.toStdString(), H5F_ACC_RDONLY);
        H5::DataSet dataset = file.openDataSet(dataset_path.toStdString());
        
        // Get dataspace and dimensions
        H5::DataSpace space = dataset.getSpace();
        int ndims = space.getSimpleExtentNdims();
        
        if (ndims == 0) {
            // Scalar dataset
            _dimensions = {1};
            _num_rows = 1;
            _num_cols = 1;
            _total_elements = 1;
        } else {
            _dimensions.resize(static_cast<size_t>(ndims));
            space.getSimpleExtentDims(_dimensions.data());
            
            _total_elements = 1;
            for (auto dim : _dimensions) {
                _total_elements *= static_cast<qint64>(dim);
            }
            
            // Determine table layout based on dimensionality
            if (ndims == 1) {
                _num_rows = static_cast<qint64>(_dimensions[0]);
                _num_cols = 1;
            } else if (ndims == 2) {
                _num_rows = static_cast<qint64>(_dimensions[0]);
                _num_cols = static_cast<int>(_dimensions[1]);
            } else {
                // For ND arrays, flatten to 2D: first dim is rows, rest flattened to cols
                _num_rows = static_cast<qint64>(_dimensions[0]);
                _num_cols = 1;
                for (size_t i = 1; i < _dimensions.size(); ++i) {
                    _num_cols *= static_cast<int>(_dimensions[i]);
                }
            }
        }
        
        // Get data type info
        H5::DataType dtype = dataset.getDataType();
        _type_class = dtype.getClass();
        _type_size = dtype.getSize();
        
        if (_type_class == H5T_INTEGER) {
            H5::IntType int_type(dtype.getId());
            _is_signed = (int_type.getSign() != H5T_SGN_NONE);
        }
        
        _is_string = (_type_class == H5T_STRING);
        
        _file_path = file_path;
        _dataset_path = dataset_path;
        _has_dataset = true;
        
        file.close();
        
    } catch (H5::Exception const & e) {
        QString msg = QString("Failed to load dataset: %1")
                      .arg(QString::fromStdString(e.getDetailMsg()));
        emit loadError(msg);
        endResetModel();
        return false;
    }
    
    endResetModel();
    emit datasetLoaded(_num_rows, _num_cols);
    return true;
}

void HDF5DatasetPreviewModel::clear() {
    _file_path.clear();
    _dataset_path.clear();
    _has_dataset = false;
    _num_rows = 0;
    _num_cols = 0;
    _total_elements = 0;
    _dimensions.clear();
    _type_class = -1;
    _type_size = 0;
    _is_signed = true;
    _is_string = false;
    _chunk_cache.clear();
}

void HDF5DatasetPreviewModel::setChunkSize(int size) {
    _chunk_size = std::max(10, size);
    _chunk_cache.clear();  // Invalidate cache when chunk size changes
}

int HDF5DatasetPreviewModel::rowCount(QModelIndex const & parent) const {
    if (parent.isValid()) {
        return 0;
    }
    // Limit displayed rows to avoid UI issues with very large datasets
    constexpr qint64 max_display_rows = 1000000;
    return static_cast<int>(std::min(_num_rows, max_display_rows));
}

int HDF5DatasetPreviewModel::columnCount(QModelIndex const & parent) const {
    if (parent.isValid()) {
        return 0;
    }
    // Limit columns to reasonable number for display
    constexpr int max_display_cols = 100;
    return std::min(_num_cols, max_display_cols);
}

QVariant HDF5DatasetPreviewModel::data(QModelIndex const & index, int role) const {
    if (!index.isValid() || !_has_dataset) {
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
    HDF5PreviewDataChunk * chunk = _chunk_cache.object(chunk_idx);
    
    if (!chunk) {
        chunk = loadChunk(chunk_idx * _chunk_size);
        if (!chunk) {
            return QVariant("Error");
        }
        _chunk_cache.insert(chunk_idx, chunk);
    }
    
    // Get data from chunk
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

QVariant HDF5DatasetPreviewModel::headerData(int section, Qt::Orientation orientation, 
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

qint64 HDF5DatasetPreviewModel::chunkIndexForRow(qint64 row) const {
    return row / _chunk_size;
}

HDF5PreviewDataChunk * HDF5DatasetPreviewModel::loadChunk(qint64 start_row) const {
    qint64 rows_to_load = std::min(static_cast<qint64>(_chunk_size), _num_rows - start_row);
    
    if (rows_to_load <= 0) {
        return nullptr;
    }
    
    auto data = readHDF5Data(start_row, rows_to_load);
    if (data.empty()) {
        return nullptr;
    }
    
    auto * chunk = new HDF5PreviewDataChunk();
    chunk->start_row = start_row;
    chunk->rows = std::move(data);
    
    return chunk;
}

std::vector<std::vector<QVariant>> HDF5DatasetPreviewModel::readHDF5Data(
    qint64 start_row, qint64 num_rows) const {
    
    std::vector<std::vector<QVariant>> result;
    
    if (!_has_dataset || num_rows <= 0) {
        return result;
    }
    
    try {
        H5::H5File file(_file_path.toStdString(), H5F_ACC_RDONLY);
        H5::DataSet dataset = file.openDataSet(_dataset_path.toStdString());
        H5::DataSpace file_space = dataset.getSpace();
        H5::DataType dtype = dataset.getDataType();
        
        // Create hyperslab selection for the rows we need
        int ndims = static_cast<int>(_dimensions.size());
        std::vector<hsize_t> offset(static_cast<size_t>(ndims), 0);
        std::vector<hsize_t> count(_dimensions);
        
        if (ndims >= 1) {
            offset[0] = static_cast<hsize_t>(start_row);
            count[0] = static_cast<hsize_t>(num_rows);
        }
        
        file_space.selectHyperslab(H5S_SELECT_SET, count.data(), offset.data());
        
        // Create memory dataspace
        H5::DataSpace mem_space(ndims, count.data());
        
        // Calculate total elements to read
        hsize_t total_read = 1;
        for (auto c : count) {
            total_read *= c;
        }
        
        // Read data based on type
        result.reserve(static_cast<size_t>(num_rows));
        
        if (_type_class == H5T_FLOAT) {
            if (_type_size == 4) {
                std::vector<float> buffer(total_read);
                dataset.read(buffer.data(), H5::PredType::NATIVE_FLOAT, mem_space, file_space);
                
                size_t idx = 0;
                for (qint64 r = 0; r < num_rows && idx < buffer.size(); ++r) {
                    std::vector<QVariant> row;
                    row.reserve(static_cast<size_t>(_num_cols));
                    for (int c = 0; c < _num_cols && idx < buffer.size(); ++c) {
                        row.emplace_back(static_cast<double>(buffer[idx++]));
                    }
                    result.push_back(std::move(row));
                }
            } else {
                std::vector<double> buffer(total_read);
                dataset.read(buffer.data(), H5::PredType::NATIVE_DOUBLE, mem_space, file_space);
                
                size_t idx = 0;
                for (qint64 r = 0; r < num_rows && idx < buffer.size(); ++r) {
                    std::vector<QVariant> row;
                    row.reserve(static_cast<size_t>(_num_cols));
                    for (int c = 0; c < _num_cols && idx < buffer.size(); ++c) {
                        row.emplace_back(buffer[idx++]);
                    }
                    result.push_back(std::move(row));
                }
            }
        } else if (_type_class == H5T_INTEGER) {
            if (_is_signed) {
                if (_type_size <= 4) {
                    std::vector<int32_t> buffer(total_read);
                    dataset.read(buffer.data(), H5::PredType::NATIVE_INT32, mem_space, file_space);
                    
                    size_t idx = 0;
                    for (qint64 r = 0; r < num_rows && idx < buffer.size(); ++r) {
                        std::vector<QVariant> row;
                        row.reserve(static_cast<size_t>(_num_cols));
                        for (int c = 0; c < _num_cols && idx < buffer.size(); ++c) {
                            row.emplace_back(buffer[idx++]);
                        }
                        result.push_back(std::move(row));
                    }
                } else {
                    std::vector<int64_t> buffer(total_read);
                    dataset.read(buffer.data(), H5::PredType::NATIVE_INT64, mem_space, file_space);
                    
                    size_t idx = 0;
                    for (qint64 r = 0; r < num_rows && idx < buffer.size(); ++r) {
                        std::vector<QVariant> row;
                        row.reserve(static_cast<size_t>(_num_cols));
                        for (int c = 0; c < _num_cols && idx < buffer.size(); ++c) {
                            row.emplace_back(static_cast<qlonglong>(buffer[idx++]));
                        }
                        result.push_back(std::move(row));
                    }
                }
            } else {
                if (_type_size <= 4) {
                    std::vector<uint32_t> buffer(total_read);
                    dataset.read(buffer.data(), H5::PredType::NATIVE_UINT32, mem_space, file_space);
                    
                    size_t idx = 0;
                    for (qint64 r = 0; r < num_rows && idx < buffer.size(); ++r) {
                        std::vector<QVariant> row;
                        row.reserve(static_cast<size_t>(_num_cols));
                        for (int c = 0; c < _num_cols && idx < buffer.size(); ++c) {
                            row.emplace_back(buffer[idx++]);
                        }
                        result.push_back(std::move(row));
                    }
                } else {
                    std::vector<uint64_t> buffer(total_read);
                    dataset.read(buffer.data(), H5::PredType::NATIVE_UINT64, mem_space, file_space);
                    
                    size_t idx = 0;
                    for (qint64 r = 0; r < num_rows && idx < buffer.size(); ++r) {
                        std::vector<QVariant> row;
                        row.reserve(static_cast<size_t>(_num_cols));
                        for (int c = 0; c < _num_cols && idx < buffer.size(); ++c) {
                            row.emplace_back(static_cast<qulonglong>(buffer[idx++]));
                        }
                        result.push_back(std::move(row));
                    }
                }
            }
        } else if (_type_class == H5T_STRING) {
            // Handle variable-length strings
            if (dtype.isVariableStr()) {
                std::vector<char *> buffer(total_read, nullptr);
                H5::StrType str_type(H5::PredType::C_S1, H5T_VARIABLE);
                dataset.read(buffer.data(), str_type, mem_space, file_space);
                
                size_t idx = 0;
                for (qint64 r = 0; r < num_rows && idx < buffer.size(); ++r) {
                    std::vector<QVariant> row;
                    row.reserve(static_cast<size_t>(_num_cols));
                    for (int c = 0; c < _num_cols && idx < buffer.size(); ++c) {
                        if (buffer[idx]) {
                            row.emplace_back(QString::fromUtf8(buffer[idx]));
                        } else {
                            row.emplace_back(QString());
                        }
                        ++idx;
                    }
                    result.push_back(std::move(row));
                }
                
                // Free HDF5-allocated strings
                H5::DataSet::vlenReclaim(buffer.data(), str_type, mem_space);
            } else {
                // Fixed-length strings
                size_t str_size = dtype.getSize();
                std::vector<char> buffer(total_read * str_size);
                dataset.read(buffer.data(), dtype, mem_space, file_space);
                
                size_t idx = 0;
                for (qint64 r = 0; r < num_rows && idx < total_read; ++r) {
                    std::vector<QVariant> row;
                    row.reserve(static_cast<size_t>(_num_cols));
                    for (int c = 0; c < _num_cols && idx < total_read; ++c) {
                        QString str = QString::fromUtf8(buffer.data() + idx * str_size, 
                                                        static_cast<int>(str_size)).trimmed();
                        row.emplace_back(str);
                        ++idx;
                    }
                    result.push_back(std::move(row));
                }
            }
        } else {
            // Unsupported type - show placeholder
            for (qint64 r = 0; r < num_rows; ++r) {
                std::vector<QVariant> row;
                row.reserve(static_cast<size_t>(_num_cols));
                for (int c = 0; c < _num_cols; ++c) {
                    row.emplace_back(tr("<unsupported>"));
                }
                result.push_back(std::move(row));
            }
        }
        
        file.close();
        
    } catch (H5::Exception const & e) {
        // Return empty on error
        result.clear();
    }
    
    return result;
}
