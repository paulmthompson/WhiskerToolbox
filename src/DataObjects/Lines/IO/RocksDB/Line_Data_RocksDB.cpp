
#include "Line_Data_RocksDB.hpp"
#include "IO/CapnProto/line_data.capnp.h" // Generated Cap'n Proto header
#include "IO/CapnProto/Serialization.hpp"

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>

#include <capnp/message.h>
#include <capnp/serialize.h>
#include <kj/array.h>

#include <vector>
#include <iostream> // For error messages

std::string frameIdToStorageKey(const std::string& prefix, int frame_id) {
    return prefix + std::to_string(frame_id);
}

RocksDBLineDataStorage::RocksDBLineDataStorage() = default;
RocksDBLineDataStorage::~RocksDBLineDataStorage() = default;

std::string RocksDBLineDataStorage::serializeImageSizeProto(const ImageSize& image_size) {
    capnp::MallocMessageBuilder message;

    LineDataProto::Builder meta_builder = message.initRoot<LineDataProto>();
    meta_builder.setImageWidth(static_cast<uint32_t>(image_size.width));
    meta_builder.setImageHeight(static_cast<uint32_t>(image_size.height));

    kj::Array<capnp::word> words = capnp::messageToFlatArray(message);
    kj::ArrayPtr<char> chars = words.asChars();
    return std::string(chars.begin(), chars.size());
}

ImageSize RocksDBLineDataStorage::deserializeImageSizeProto(const std::string& serialized_data) {
    kj::ArrayPtr<const capnp::word> words(
            reinterpret_cast<const capnp::word*>(serialized_data.data()),
            serialized_data.size() / sizeof(capnp::word)
            );
    capnp::FlatArrayMessageReader reader(words);
    LineDataProto::Reader meta_reader = reader.getRoot<LineDataProto>();

    ImageSize cpp_image_size;
    cpp_image_size.width = static_cast<int>(meta_reader.getImageWidth());
    cpp_image_size.height = static_cast<int>(meta_reader.getImageHeight());
    return cpp_image_size;
}


bool RocksDBLineDataStorage::save(const LineData& data, const std::string& db_path) {
    rocksdb::DB* db;
    rocksdb::Options options;
    options.create_if_missing = true;
    rocksdb::Status status = rocksdb::DB::Open(options, db_path, &db);

    if (!status.ok()) {
        std::cerr << "RocksDB: Unable to open/create database at " << db_path << ": " << status.ToString() << std::endl;
        return false;
    }
    std::unique_ptr<rocksdb::DB> db_ptr(db);

    // --- Save ImageSize ---
    std::string serialized_is = serializeImageSizeProto(data.getImageSize());
    status = db_ptr->Put(rocksdb::WriteOptions(), KEY_IMAGESIZE, serialized_is);
    if (!status.ok()) {
        std::cerr << "RocksDB: Failed to save ImageSize: " << status.ToString() << std::endl;
        return false;
    }

    // --- Save frame data (each frame as a separate K-V pair) ---
    // Your `Line_Data_CAPNP.cpp` already has logic to iterate frames and lines.
    // We will adapt that to serialize each frame's lines individually.

    const auto& all_frames_map = data.getData(); // std::map<int, std::vector<Line2D>>
    for (const auto& pair : all_frames_map) {
        int frame_id = pair.first;
        const std::vector<Line2D>& cpp_lines_for_frame = pair.second;

        capnp::MallocMessageBuilder frame_message;
        // We need a root for this message. TimeLine seems to represent a frame's data well.
        // Alternatively, if TimeLine includes the 'time' field and we don't want it in the blob
        // (since 'time' is the key), we'd make a new Cap'n Proto struct like `FrameLinesOnly { lines @0: List<Line>; }`
        // Using TimeLine for now, but only populating its 'lines' part as 'time' is the key.
        TimeLine::Builder capnp_timeline_builder = frame_message.initRoot<TimeLine>();
        // capnp_timeline_builder.setTime(frame_id); // Optional: can also store time in blob if desired

        auto capnp_lines_list = capnp_timeline_builder.initLines(cpp_lines_for_frame.size());
        for (size_t i = 0; i < cpp_lines_for_frame.size(); ++i) {
            const auto& cpp_line = cpp_lines_for_frame[i]; // Line2D, which is std::vector<Point2D<float>>
            auto capnp_line_builder = capnp_lines_list[i];
            auto capnp_points_list = capnp_line_builder.initPoints(cpp_line.size());
            for (size_t j = 0; j < cpp_line.size(); ++j) {
                const auto& cpp_point = cpp_line[j]; // Point2D<float>
                auto capnp_point_builder = capnp_points_list[j];
                capnp_point_builder.setX(cpp_point.x);
                capnp_point_builder.setY(cpp_point.y);
            }
        }

        kj::Array<capnp::word> words = capnp::messageToFlatArray(frame_message);
        kj::ArrayPtr<char> chars = words.asChars();
        std::string value_str(chars.begin(), chars.size());
        std::string key_str = frameIdToStorageKey(FRAME_KEY_PREFIX, frame_id);

        status = db_ptr->Put(rocksdb::WriteOptions(), key_str, value_str);
        if (!status.ok()) {
            std::cerr << "RocksDB: Failed to save frame " << frame_id << ": " << status.ToString() << std::endl;
            return false;
        }
    }
    return true;
}

bool RocksDBLineDataStorage::load(LineData& data_to_populate, const std::string& db_path) {
    rocksdb::DB* db;
    rocksdb::Options options;
    rocksdb::Status status = rocksdb::DB::Open(options, db_path, &db); // For read-only, set options.create_if_missing = false;

    if (!status.ok()) {
        std::cerr << "RocksDB: Unable to open database at " << db_path << ": " << status.ToString() << std::endl;
        return false;
    }
    std::unique_ptr<rocksdb::DB> db_ptr(db);

    std::map<int, std::vector<Line2D>> loaded_data_map;

    // --- Load ImageSize ---
    std::string serialized_is_value;
    status = db_ptr->Get(rocksdb::ReadOptions(), KEY_IMAGESIZE, &serialized_is_value);
    if (status.ok()) {
        data_to_populate.setImageSize(deserializeImageSizeProto(serialized_is_value));
    } else if (!status.IsNotFound()) {
        std::cerr << "RocksDB: Failed to load ImageSize: " << status.ToString() << std::endl;
        return false;
    } // else: not found, use default or handle as error if required

    // --- Load LockState ---
    // You would implement deserializeLockStateProto and use it here
    // std::string serialized_ls_value;
    // status = db_ptr->Get(rocksdb::ReadOptions(), KEY_LOCKSTATE, &serialized_ls_value);
    // if (status.ok()) { ... deserialize and set ... }


    // --- Load frame data ---
    rocksdb::Iterator* it = db_ptr->NewIterator(rocksdb::ReadOptions());
    std::unique_ptr<rocksdb::Iterator> it_ptr(it);

    for (it_ptr->SeekToFirst(); it_ptr->Valid(); it_ptr->Next()) {
        std::string key_str = it_ptr->key().ToString();
        if (key_str.rfind(FRAME_KEY_PREFIX, 0) == 0) { // Check prefix
            try {
                int frame_id = std::stoi(key_str.substr(FRAME_KEY_PREFIX.length()));
                std::string value_str = it_ptr->value().ToString();

                kj::ArrayPtr<const capnp::word> words(
                        reinterpret_cast<const capnp::word*>(value_str.data()),
                        value_str.size() / sizeof(capnp::word)
                        );
                capnp::FlatArrayMessageReader frame_reader_msg(words);
                TimeLine::Reader capnp_timeline_reader = frame_reader_msg.getRoot<TimeLine>();

                std::vector<Line2D> cpp_lines_for_frame;
                cpp_lines_for_frame.reserve(capnp_timeline_reader.getLines().size());

                for (Line::Reader capnp_line_reader : capnp_timeline_reader.getLines()) {
                    Line2D cpp_line;
                    cpp_line.reserve(capnp_line_reader.getPoints().size());
                    for (Point::Reader capnp_point_reader : capnp_line_reader.getPoints()) {
                        cpp_line.push_back({capnp_point_reader.getX(), capnp_point_reader.getY()});
                    }
                    cpp_lines_for_frame.push_back(std::move(cpp_line));
                }
                loaded_data_map[frame_id] = std::move(cpp_lines_for_frame);

            } catch (const std::exception& e) { // Catch stoi errors or capnp errors
                std::cerr << "RocksDB: Error processing frame for key " << key_str << ": " << e.what() << std::endl;
            }
        }
    }
    if (!it_ptr->status().ok()) {
        std::cerr << "RocksDB: Iterator error: " << it_ptr->status().ToString() << std::endl;
        return false;
    }

    // Populate the LineData object using its constructor or a setData method
    data_to_populate = LineData(loaded_data_map);
    // Re-apply metadata if it was not set on the 'data_to_populate' directly earlier
    // (e.g. if LineData constructor doesn't take ImageSize/LockState or resets them)
    // For instance, if you loaded ImageSize into a temp variable:
    // ImageSize loaded_is = ...; data_to_populate.setImageSize(loaded_is);

    return true;
}
