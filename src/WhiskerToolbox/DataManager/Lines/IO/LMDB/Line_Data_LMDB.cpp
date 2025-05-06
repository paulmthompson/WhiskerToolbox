
#include "Line_Data_LMDB.hpp"

#include "Lines/IO/LMDB/line_data.capnp.h"

#include <capnp/message.h>
#include <capnp/serialize.h>
#include <lmdb.h>
#include <kj/std/iostream.h>

#include <algorithm> //find
#include <iostream>
#include <vector>

namespace {
// Helper function to initialize LMDB environment
MDB_env* initLMDBEnv(const std::string& dbPath, bool readOnly = false) {
    MDB_env* env;
    int rc = mdb_env_create(&env);
    if (rc != 0) {
        std::cerr << "Failed to create LMDB environment: " << mdb_strerror(rc) << std::endl;
        return nullptr;
    }

    // Set max DB size (10GB)
    mdb_env_set_mapsize(env, 10ULL * 1024ULL * 1024ULL * 1024ULL);

    unsigned int flags = 0;
    if (readOnly) {
        flags = MDB_RDONLY;
    }

    rc = mdb_env_open(env, dbPath.c_str(), flags, 0664);
    if (rc != 0) {
        std::cerr << "Failed to open LMDB environment: " << mdb_strerror(rc) << std::endl;
        mdb_env_close(env);
        return nullptr;
    }

    return env;
}

// Convert LineData to Cap'n Proto message
kj::ArrayPtr<kj::byte> serializeLineData(const LineData* lineData) {
    capnp::MallocMessageBuilder message;
    LineDataProto::Builder lineDataProto = message.initRoot<LineDataProto>();

    std::vector<int> times = lineData->getTimesWithLines();

    auto timeLinesList = lineDataProto.initTimeLines(times.size());

    // Fill in the data
    for (size_t i = 0; i < times.size(); i++) {
        int time = times[i];
        auto timeLine = timeLinesList[i];

        // Set the time
        timeLine.setTime(time);

        // Get the lines for this time
        std::vector<Line2D> const & lines = lineData->getLinesAtTime(time);

        // Initialize the lines list with the correct size
        auto linesList = timeLine.initLines(lines.size());

        // Fill in each line
        for (size_t j = 0; j < lines.size(); j++) {
            Line2D const & line = lines[j];
            auto lineBuilder = linesList[j];

            // Initialize points list
            auto pointsList = lineBuilder.initPoints(line.size());

            // Fill in each point
            for (size_t k = 0; k < line.size(); k++) {
                auto pointBuilder = pointsList[k];
                pointBuilder.setX(line[k].x);
                pointBuilder.setY(line[k].y);
            }
        }
    }

    // Set image size if available
    ImageSize imgSize = lineData->getImageSize();
    if (imgSize.width > 0 && imgSize.height > 0) {
        lineDataProto.setImageWidth(static_cast<uint32_t>(imgSize.width));
        lineDataProto.setImageHeight(static_cast<uint32_t>(imgSize.height));
    }

    // 2. Serialize the message to a flat byte array
    kj::Array<capnp::word> words = capnp::messageToFlatArray(message);
    kj::ArrayPtr<kj::byte> buffer = words.asBytes();
    return buffer;
}

// Convert Cap'n Proto message to LineData
std::shared_ptr<LineData> deserializeLineData(kj::ArrayPtr<const capnp::word> messageData) {
    capnp::FlatArrayMessageReader message(messageData);
    LineDataProto::Reader lineDataProto = message.getRoot<LineDataProto>();

    // Create a new LineData object
    auto lineData = std::make_shared<LineData>();

    // Read image size if available
    uint32_t width = lineDataProto.getImageWidth();
    uint32_t height = lineDataProto.getImageHeight();
    if (width > 0 && height > 0) {
        lineData->setImageSize(ImageSize{static_cast<int>(width), static_cast<int>(height)});
    }

    // Process all time lines
    std::map<int, std::vector<Line2D>> dataMap;
    for (auto timeLine : lineDataProto.getTimeLines()) {
        int time = timeLine.getTime();
        std::vector<Line2D> lines;

        for (auto line : timeLine.getLines()) {
            Line2D currentLine;

            for (auto point : line.getPoints()) {
                currentLine.push_back(Point2D<float>{point.getX(), point.getY()});
            }

            lines.push_back(currentLine);
        }

        dataMap[time] = lines;
    }

    // Create a new LineData with the deserialized data
    return std::make_shared<LineData>(dataMap);
}
}

bool saveLineDataToLMDB(const LineData* lineData, const std::string& dbPath, const std::string& key) {
    MDB_env* env = initLMDBEnv(dbPath);
    if (!env) return false;

    MDB_txn* txn;
    MDB_dbi dbi;
    int rc = mdb_txn_begin(env, nullptr, 0, &txn);
    if (rc != 0) {
        std::cerr << "Failed to begin transaction: " << mdb_strerror(rc) << std::endl;
        mdb_env_close(env);
        return false;
    }

    rc = mdb_dbi_open(txn, nullptr, 0, &dbi);
    if (rc != 0) {
        std::cerr << "Failed to open database: " << mdb_strerror(rc) << std::endl;
        mdb_txn_abort(txn);
        mdb_env_close(env);
        return false;
    }

    // Serialize data
    auto buffer = serializeLineData(lineData);

    // Set up LMDB values
    MDB_val dbKey, dbValue;
    dbKey.mv_size = key.size();
    dbKey.mv_data = const_cast<char*>(key.c_str());
    dbValue.mv_size = buffer.size();
    dbValue.mv_data = reinterpret_cast<void*>(buffer.begin());

    // Put data into LMDB
    rc = mdb_put(txn, dbi, &dbKey, &dbValue, 0);
    if (rc != 0) {
        std::cerr << "Failed to store data: " << mdb_strerror(rc) << std::endl;
        mdb_txn_abort(txn);
        mdb_env_close(env);
        return false;
    }

    // Commit transaction
    rc = mdb_txn_commit(txn);
    if (rc != 0) {
        std::cerr << "Failed to commit transaction: " << mdb_strerror(rc) << std::endl;
        mdb_env_close(env);
        return false;
    }

    mdb_env_close(env);
    return true;
}

std::shared_ptr<LineData> loadLineDataFromLMDB(const std::string& dbPath, const std::string& key) {
    MDB_env* env = initLMDBEnv(dbPath, true); // Open in read-only mode
    if (!env) return nullptr;

    MDB_txn* txn;
    MDB_dbi dbi;
    int rc = mdb_txn_begin(env, nullptr, MDB_RDONLY, &txn);
    if (rc != 0) {
        std::cerr << "Failed to begin transaction: " << mdb_strerror(rc) << std::endl;
        mdb_env_close(env);
        return nullptr;
    }

    rc = mdb_dbi_open(txn, nullptr, 0, &dbi);
    if (rc != 0) {
        std::cerr << "Failed to open database: " << mdb_strerror(rc) << std::endl;
        mdb_txn_abort(txn);
        mdb_env_close(env);
        return nullptr;
    }

    // Set up key for lookup
    MDB_val dbKey, dbValue;
    dbKey.mv_size = key.size();
    dbKey.mv_data = const_cast<char*>(key.c_str());

    // Get data from LMDB
    rc = mdb_get(txn, dbi, &dbKey, &dbValue);
    if (rc != 0) {
        std::cerr << "Failed to retrieve data: " << mdb_strerror(rc) << std::endl;
        mdb_txn_abort(txn);
        mdb_env_close(env);
        return nullptr;
    }

    // Create array pointer for Cap'n Proto deserialization
    kj::ArrayPtr<const capnp::word> messageData(
            reinterpret_cast<const capnp::word*>(dbValue.mv_data),
            dbValue.mv_size / sizeof(capnp::word)
            );

    // Deserialize
    auto lineData = deserializeLineData(messageData);

    // Cleanup
    mdb_txn_abort(txn);
    mdb_env_close(env);

    return lineData;
}

bool updateLineDataTimeFrames(const std::string& dbPath,
                              std::string& key,
                              std::map<int, std::vector<Line2D>>& timeFrames) {
    // First, load the existing data
    auto lineData = loadLineDataFromLMDB(dbPath, key);
    if (!lineData) {
        std::cerr << "Failed to load LineData for updating" << std::endl;
        return false;
    }

    // Update the time frames
    for (const auto& [time, lines] : timeFrames) {
        // Clear existing lines at this time
        lineData->clearLinesAtTime(time);

        // Add the updated lines
        for (const auto& line : lines) {
            lineData->addLineAtTime(time, line);
        }
    }

    // Save the updated data back to LMDB
    return saveLineDataToLMDB(lineData.get(), dbPath, key);
}

std::map<int, std::vector<Line2D>> getLineDataTimeFrames(const std::string& dbPath,
                                                         std::string& key,
                                                         std::vector<int>& times) {
    std::map<int, std::vector<Line2D>> result;

    // Load the entire data (unfortunately LMDB doesn't support partial reads easily)
    auto lineData = loadLineDataFromLMDB(dbPath, key);
    if (!lineData) {
        std::cerr << "Failed to load LineData for retrieval" << std::endl;
        return result;
    }

    // Extract only the requested time frames
    for (int time : times) {
        if (std::find(lineData->getTimesWithLines().begin(),
                      lineData->getTimesWithLines().end(), time)
            != lineData->getTimesWithLines().end()) {
            result[time] = lineData->getLinesAtTime(time);
        }
    }

    return result;
}
