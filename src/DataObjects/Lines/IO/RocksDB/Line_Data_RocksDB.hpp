#ifndef LINE_DATA_ROCKSDB_HPP
#define LINE_DATA_ROCKSDB_HPP

#include "Lines/Line_Data.hpp" // Your existing LineData header

#include <memory> // For std::unique_ptr
#include <string>

// Forward declare RocksDB
namespace rocksdb { class DB; }

#include "Lines/IO/CAPNP/line_data.capnp.h"

class RocksDBLineDataStorage {
public:
    RocksDBLineDataStorage();
    ~RocksDBLineDataStorage();

    bool save(const LineData& data, const std::string& db_path);
    bool load(LineData& data_to_populate, const std::string& db_path);

private:
    const std::string KEY_IMAGESIZE = "__METADATA_IMAGESIZE__";
    const std::string FRAME_KEY_PREFIX = "frame_";

    std::string serializeImageSizeProto(const ImageSize& image_size);
    ImageSize deserializeImageSizeProto(const std::string& serialized_data);
};

#endif// LINE_DATA_ROCKSDB_HPP
