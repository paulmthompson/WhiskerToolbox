/**
 * @file DataManagerSnapshot.cpp
 * @brief Implementation of DataManager state snapshotting for golden trace tests
 */

#include "DataManagerSnapshot.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "Tensors/TensorData.hpp"

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace {

// FNV-1a 64-bit hash
constexpr uint64_t fnv_offset_basis = 14695981039346656037ULL;
constexpr uint64_t fnv_prime = 1099511628211ULL;

class FnvHasher {
public:
    void feedBytes(void const * data, std::size_t len) {
        auto const * ptr = static_cast<uint8_t const *>(data);
        for (std::size_t i = 0; i < len; ++i) {
            _hash ^= ptr[i];
            _hash *= fnv_prime;
        }
    }

    void feedFloat(float v) {
        // Normalise negative zero to positive zero for determinism
        if (v == 0.0f) v = 0.0f;
        feedBytes(&v, sizeof(v));
    }

    void feedInt64(int64_t v) { feedBytes(&v, sizeof(v)); }

    void feedUint64(uint64_t v) { feedBytes(&v, sizeof(v)); }

    [[nodiscard]] std::string hexDigest() const {
        std::ostringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(16) << _hash;
        return ss.str();
    }

private:
    uint64_t _hash = fnv_offset_basis;
};

// ---------- Entity count helpers ----------

std::size_t entityCount(std::shared_ptr<PointData> const & d) {
    return d ? d->getTotalEntryCount() : 0;
}

std::size_t entityCount(std::shared_ptr<LineData> const & d) {
    return d ? d->getTotalEntryCount() : 0;
}

std::size_t entityCount(std::shared_ptr<MaskData> const & d) {
    return d ? d->getTotalEntryCount() : 0;
}

std::size_t entityCount(std::shared_ptr<AnalogTimeSeries> const & d) {
    return d ? d->getNumSamples() : 0;
}

std::size_t entityCount(std::shared_ptr<RaggedAnalogTimeSeries> const & d) {
    return d ? d->getTotalValueCount() : 0;
}

std::size_t entityCount(std::shared_ptr<DigitalEventSeries> const & d) {
    return d ? d->size() : 0;
}

std::size_t entityCount(std::shared_ptr<DigitalIntervalSeries> const & d) {
    return d ? d->size() : 0;
}

std::size_t entityCount(std::shared_ptr<TensorData> const & d) {
    return d ? d->numRows() : 0;
}

// ---------- Content hash helpers ----------

std::string contentHash(std::shared_ptr<PointData> const & d) {
    FnvHasher h;
    if (!d) return h.hexDigest();
    for (auto const & [time, entity_id, point]: d->flattened_data()) {
        h.feedInt64(time.getValue());
        h.feedFloat(point.x);
        h.feedFloat(point.y);
    }
    return h.hexDigest();
}

std::string contentHash(std::shared_ptr<LineData> const & d) {
    FnvHasher h;
    if (!d) return h.hexDigest();
    for (auto const & [time, entity_id, line]: d->flattened_data()) {
        h.feedInt64(time.getValue());
        for (auto const & pt: line) {
            h.feedFloat(pt.x);
            h.feedFloat(pt.y);
        }
    }
    return h.hexDigest();
}

std::string contentHash(std::shared_ptr<MaskData> const & d) {
    FnvHasher h;
    if (!d) return h.hexDigest();
    for (auto const & [time, entity_id, mask]: d->flattened_data()) {
        h.feedInt64(time.getValue());
        for (auto const & pt: mask) {
            auto x = static_cast<uint32_t>(pt.x);
            auto y = static_cast<uint32_t>(pt.y);
            h.feedBytes(&x, sizeof(x));
            h.feedBytes(&y, sizeof(y));
        }
    }
    return h.hexDigest();
}

std::string contentHash(std::shared_ptr<AnalogTimeSeries> const & d) {
    FnvHasher h;
    if (!d) return h.hexDigest();
    for (auto const & sample: d->view()) {
        h.feedInt64(sample.time().getValue());
        h.feedFloat(sample.value());
    }
    return h.hexDigest();
}

std::string contentHash(std::shared_ptr<RaggedAnalogTimeSeries> const & d) {
    FnvHasher h;
    if (!d) return h.hexDigest();
    for (auto const & [time, value]: d->elements()) {
        h.feedInt64(time.getValue());
        h.feedFloat(value);
    }
    return h.hexDigest();
}

std::string contentHash(std::shared_ptr<DigitalEventSeries> const & d) {
    FnvHasher h;
    if (!d) return h.hexDigest();
    for (auto const & elem: d->view()) {
        h.feedInt64(elem.time().getValue());
    }
    return h.hexDigest();
}

std::string contentHash(std::shared_ptr<DigitalIntervalSeries> const & d) {
    FnvHasher h;
    if (!d) return h.hexDigest();
    for (auto const & elem: d->view()) {
        h.feedInt64(elem.value().start);
        h.feedInt64(elem.value().end);
    }
    return h.hexDigest();
}

std::string contentHash(std::shared_ptr<TensorData> const & d) {
    FnvHasher h;
    if (!d) return h.hexDigest();
    // Hash rows × columns values
    auto rows = d->numRows();
    auto cols = d->numColumns();
    h.feedUint64(rows);
    h.feedUint64(cols);
    // TensorData row-level access is complex; hash the row/column counts
    // as a lightweight fingerprint for now
    return h.hexDigest();
}

}// namespace

DataManagerSnapshot snapshotDataManager(DataManager & dm) {
    DataManagerSnapshot snap;

    auto const keys = dm.getAllKeys();
    for (auto const & key: keys) {
        // Skip the default media key — it's always present and not command-managed
        if (key == "media") continue;

        auto const type = dm.getType(key);
        snap.key_type[key] = convert_data_type_to_string(type);

        switch (type) {
            case DM_DataType::Points: {
                auto d = dm.getData<PointData>(key);
                snap.key_entity_count[key] = entityCount(d);
                snap.key_content_hash[key] = contentHash(d);
                break;
            }
            case DM_DataType::Line: {
                auto d = dm.getData<LineData>(key);
                snap.key_entity_count[key] = entityCount(d);
                snap.key_content_hash[key] = contentHash(d);
                break;
            }
            case DM_DataType::Mask: {
                auto d = dm.getData<MaskData>(key);
                snap.key_entity_count[key] = entityCount(d);
                snap.key_content_hash[key] = contentHash(d);
                break;
            }
            case DM_DataType::Analog: {
                auto d = dm.getData<AnalogTimeSeries>(key);
                snap.key_entity_count[key] = entityCount(d);
                snap.key_content_hash[key] = contentHash(d);
                break;
            }
            case DM_DataType::RaggedAnalog: {
                auto d = dm.getData<RaggedAnalogTimeSeries>(key);
                snap.key_entity_count[key] = entityCount(d);
                snap.key_content_hash[key] = contentHash(d);
                break;
            }
            case DM_DataType::DigitalEvent: {
                auto d = dm.getData<DigitalEventSeries>(key);
                snap.key_entity_count[key] = entityCount(d);
                snap.key_content_hash[key] = contentHash(d);
                break;
            }
            case DM_DataType::DigitalInterval: {
                auto d = dm.getData<DigitalIntervalSeries>(key);
                snap.key_entity_count[key] = entityCount(d);
                snap.key_content_hash[key] = contentHash(d);
                break;
            }
            case DM_DataType::Tensor: {
                auto d = dm.getData<TensorData>(key);
                snap.key_entity_count[key] = entityCount(d);
                snap.key_content_hash[key] = contentHash(d);
                break;
            }
            default:
                // Video, Images, Time, Unknown — record type but skip count/hash
                snap.key_entity_count[key] = 0;
                snap.key_content_hash[key] = "";
                break;
        }
    }

    return snap;
}
