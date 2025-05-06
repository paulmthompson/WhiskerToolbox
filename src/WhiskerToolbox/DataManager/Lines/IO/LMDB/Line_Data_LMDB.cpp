
#include "Line_Data_LMDB.hpp"

#include "Lines/IO/CAPNP/Line_Data_CAPNP.hpp"

#include <capnp/message.h>
#include <capnp/serialize.h>
#include <kj/std/iostream.h>
#include <lmdb.h>

#include <algorithm>//find
#include <iostream>
#include <vector>

namespace {
// Helper function to initialize LMDB environment
MDB_env * initLMDBEnv(std::string const & dbPath, bool readOnly = false) {
    MDB_env * env;
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
}

bool saveLineDataToLMDB(LineData const * lineData, std::string const & dbPath, std::string const & key) {
    MDB_env * env = initLMDBEnv(dbPath);
    if (!env) return false;

    MDB_txn * txn;
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
    auto words = serializeLineData(lineData);
    kj::ArrayPtr<kj::byte> buffer = words.asBytes();

    // Set up LMDB values
    MDB_val dbKey, dbValue;
    dbKey.mv_size = key.size();
    dbKey.mv_data = const_cast<char *>(key.c_str());
    dbValue.mv_size = buffer.size();
    dbValue.mv_data = reinterpret_cast<void *>(buffer.begin());

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

std::shared_ptr<LineData> loadLineDataFromLMDB(std::string const & dbPath, std::string const & key) {
    MDB_env * env = initLMDBEnv(dbPath, true);// Open in read-only mode
    if (!env) return nullptr;

    MDB_txn * txn;
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
    dbKey.mv_data = const_cast<char *>(key.c_str());

    // Get data from LMDB
    rc = mdb_get(txn, dbi, &dbKey, &dbValue);
    if (rc != 0) {
        std::cerr << "Failed to retrieve data: " << mdb_strerror(rc) << std::endl;
        mdb_txn_abort(txn);
        mdb_env_close(env);
        return nullptr;
    }

    // Create array pointer for Cap'n Proto deserialization
    kj::ArrayPtr<capnp::word const> messageData(
            reinterpret_cast<capnp::word const *>(dbValue.mv_data),
            dbValue.mv_size / sizeof(capnp::word));

    // Deserialize
    auto lineData = deserializeLineData(messageData);

    // Cleanup
    mdb_txn_abort(txn);
    mdb_env_close(env);

    return lineData;
}

bool updateLineDataTimeFrames(std::string const & dbPath,
                              std::string & key,
                              std::map<int, std::vector<Line2D>> & timeFrames) {
    // First, load the existing data
    auto lineData = loadLineDataFromLMDB(dbPath, key);
    if (!lineData) {
        std::cerr << "Failed to load LineData for updating" << std::endl;
        return false;
    }

    // Update the time frames
    for (auto const & [time, lines]: timeFrames) {
        // Clear existing lines at this time
        lineData->clearLinesAtTime(time);

        // Add the updated lines
        for (auto const & line: lines) {
            lineData->addLineAtTime(time, line);
        }
    }

    // Save the updated data back to LMDB
    return saveLineDataToLMDB(lineData.get(), dbPath, key);
}

std::map<int, std::vector<Line2D>> getLineDataTimeFrames(std::string const & dbPath,
                                                         std::string & key,
                                                         std::vector<int> & times) {
    std::map<int, std::vector<Line2D>> result;

    // Load the entire data (unfortunately LMDB doesn't support partial reads easily)
    auto lineData = loadLineDataFromLMDB(dbPath, key);
    if (!lineData) {
        std::cerr << "Failed to load LineData for retrieval" << std::endl;
        return result;
    }

    // Extract only the requested time frames
    for (int const time: times) {
        if (std::find(lineData->getTimesWithLines().begin(),
                      lineData->getTimesWithLines().end(), time) != lineData->getTimesWithLines().end()) {
            result[time] = lineData->getLinesAtTime(time);
        }
    }

    return result;
}

