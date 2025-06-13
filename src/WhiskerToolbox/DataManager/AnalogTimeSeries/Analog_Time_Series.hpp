#ifndef ANALOG_TIME_SERIES_HPP
#define ANALOG_TIME_SERIES_HPP

#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrameV2.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <vector>
#include <variant>
#include <span>

/**
 * @brief The AnalogTimeSeries class
 *
 * Analog time series is used for storing continuous data
 * The data may be sampled at irregular intervals as long as the time vector is provided
 *
 */
class AnalogTimeSeries : public ObserverData {
public:
    AnalogTimeSeries();
    explicit AnalogTimeSeries(std::vector<float> analog_vector);
    AnalogTimeSeries(std::vector<float> analog_vector, std::vector<size_t> time_vector);
    explicit AnalogTimeSeries(std::map<int, float> analog_map);

    void overwriteAtTimes(std::vector<float> & analog_data, std::vector<size_t> & time);

    [[nodiscard]] float getDataAtIndex(size_t i) const { return _data[i]; };
    [[nodiscard]] size_t getTimeAtIndex(size_t i) const {
        return std::visit([i](auto const & time_storage) -> size_t {
            return time_storage.getTimeAtIndex(i);
        }, _time_storage);
    }

    [[nodiscard]] size_t getNumSamples() const { return _data.size(); };

    /**
     * @brief Get a const reference to the analog data vector
     * 
     * Returns a const reference to the internal vector containing the analog time series data values.
     * This method provides efficient read-only access to the data without copying.
     * 
     * @return const reference to std::vector<float> containing the analog data values
     * 
     * @note This method returns by const reference for performance - no data copying occurs.
     *       Use this when you need to iterate over or access the raw data values efficiently.
     * 
     * @see getTimeSeries() for accessing the corresponding time indices
     * @see getDataInRange() for accessing data within a specific time range
     */
    [[nodiscard]] std::vector<float> const & getAnalogTimeSeries() const { return _data; };

    /**
    * @brief Get a span (view) of data values in a time range using any coordinate type
    *
    * This method returns a std::span<const float> over the contiguous block of data
    * corresponding to the given coordinate range. No data is copied.
    *
    * @param start_coord Start coordinate (any coordinate type)
    * @param end_coord End coordinate (any coordinate type)
    * @return std::span<const float> view into the data in the specified range
    * @throws std::runtime_error if no TimeFrameV2 is associated or coordinate types don't match
    * @note The returned span is valid as long as the AnalogTimeSeries is not modified.
    */
    [[nodiscard]] std::span<const float> getDataSpanInCoordinateRange(TimeCoordinate start_coord, 
                                                                      TimeCoordinate end_coord) const;

    /**
     * @brief Get the time indices as a vector
     * 
     * Returns a vector containing the time indices corresponding to each analog data sample.
     * For dense time storage, this generates the indices on-demand.
     * For sparse time storage, this returns a copy of the stored indices.
     * 
     * @return std::vector<size_t> containing the time indices
     * 
     * @note For dense storage, this method has O(n) time complexity as it generates the vector.
     *       Consider using getTimeAtIndex() for single lookups or iterating with getNumSamples().
     *       For sparse storage, this returns a copy of the internal vector.
     * 
     * @see getAnalogTimeSeries() for accessing the corresponding data values
     * @see getDataInRange() for accessing time-value pairs within a specific range
     * @see getTimeAtIndex() for single index lookups
     */
    [[nodiscard]] std::vector<size_t> getTimeSeries() const {
        return std::visit([](auto const & time_storage) -> std::vector<size_t> {
            if constexpr (std::is_same_v<std::decay_t<decltype(time_storage)>, DenseTimeRange>) {
                // Generate vector for dense storage
                std::vector<size_t> result(time_storage.count);
                for (size_t i = 0; i < time_storage.count; ++i) {
                    result[i] = time_storage.start_index + i;
                }
                return result;
            } else {
                // Return copy for sparse storage
                return time_storage.indices;
            }
        }, _time_storage);
    }

    auto getDataInRange(float start_time, float stop_time) const {
        struct DataPoint {
            size_t time_idx;
            float value;
        };

        return std::views::iota(size_t{0}, getNumSamples()) |
               std::views::filter([this, start_time, stop_time](size_t i) {
                   auto transformed_time = static_cast<float>(getTimeAtIndex(i));
                   return transformed_time >= start_time && transformed_time <= stop_time;
               }) |
               std::views::transform([this](size_t i) {
                   return DataPoint{getTimeAtIndex(i), getDataAtIndex(i)};
               });
    }

    template<typename TransformFunc>
    auto getDataInRange(float start_time, float stop_time, TransformFunc time_transform) const {
        struct DataPoint {
            size_t time_idx;
            float value;
        };

        return std::views::iota(size_t{0}, getNumSamples()) |
               std::views::filter([this, start_time, stop_time, time_transform](size_t i) {
                   auto transformed_time = time_transform(getTimeAtIndex(i));
                   return transformed_time >= start_time && transformed_time <= stop_time;
               }) |
               std::views::transform([this](size_t i) {
                   return DataPoint{getTimeAtIndex(i), getDataAtIndex(i)};
               });
    }

    // ========== TimeFrameV2 Support ==========

    /**
    * @brief Set a TimeFrameV2 reference for this data series
    *
    * Associates this AnalogTimeSeries with a strongly-typed TimeFrameV2,
    * enabling type-safe time coordinate operations.
    *
    * @param timeframe The TimeFrameV2 variant to associate with this data
    */
    void setTimeFrameV2(AnyTimeFrame timeframe) {
        _timeframe_v2 = std::move(timeframe);
    }

    /**
    * @brief Get the TimeFrameV2 reference for this data series
    *
    * @return Optional TimeFrameV2 variant if set, nullopt otherwise
    */
    [[nodiscard]] std::optional<AnyTimeFrame> getTimeFrameV2() const {
        return _timeframe_v2;
    }

    /**
    * @brief Check if this series has a TimeFrameV2 reference
    *
    * @return True if a TimeFrameV2 is associated with this series
    */
    [[nodiscard]] bool hasTimeFrameV2() const {
        return _timeframe_v2.has_value();
    }

    /**
    * @brief Get the coordinate type of the associated TimeFrameV2
    *
    * @return String identifier for the coordinate type, or "none" if no TimeFrameV2 is set
    */
    [[nodiscard]] std::string getCoordinateType() const {
        if (!_timeframe_v2.has_value()) {
            return "none";
        }

        return std::visit([](auto const & timeframe_ptr) -> std::string {
            using FrameType = std::decay_t<decltype(*timeframe_ptr)>;
            using CoordType = typename FrameType::coordinate_type;
            
            if constexpr (std::is_same_v<CoordType, ClockTicks>) {
                return "ClockTicks";
            } else if constexpr (std::is_same_v<CoordType, CameraFrameIndex>) {
                return "CameraFrameIndex";
            } else if constexpr (std::is_same_v<CoordType, Seconds>) {
                return "Seconds";
            } else if constexpr (std::is_same_v<CoordType, UncalibratedIndex>) {
                return "UncalibratedIndex";
            } else {
                return "unknown";
            }
        }, _timeframe_v2.value());
    }

    /**
    * @brief Check if the TimeFrameV2 uses a specific coordinate type
    *
    * @tparam CoordinateType The coordinate type to check for
    * @return True if the TimeFrameV2 uses the specified coordinate type
    */
    template<typename CoordinateType>
    [[nodiscard]] bool hasCoordinateType() const {
        if (!_timeframe_v2.has_value()) {
            return false;
        }

        return std::visit([](auto const & timeframe_ptr) -> bool {
            using FrameType = std::decay_t<decltype(*timeframe_ptr)>;
            using FrameCoordType = typename FrameType::coordinate_type;
            return std::is_same_v<FrameCoordType, CoordinateType>;
        }, _timeframe_v2.value());
    }

    /**
    * @brief Get data values in a time range using any coordinate type
    *
    * This method automatically determines the correct coordinate type from the
    * associated TimeFrameV2 and validates that the provided coordinates match.
    * It uses std::holds_alternative internally for type safety.
    *
    * @param start_coord Start coordinate (any coordinate type)
    * @param end_coord End coordinate (any coordinate type) 
    * @return Vector of data values within the specified coordinate range
    * @throws std::runtime_error if no TimeFrameV2 is associated or coordinate types don't match
    */
    std::vector<float> getDataInCoordinateRange(TimeCoordinate start_coord, TimeCoordinate end_coord) const {
        if (!_timeframe_v2.has_value()) {
            throw std::runtime_error("No TimeFrameV2 associated with this AnalogTimeSeries");
        }

        return std::visit([&](auto const & timeframe_ptr) -> std::vector<float> {
            using FrameType = std::decay_t<decltype(*timeframe_ptr)>;
            using FrameCoordType = typename FrameType::coordinate_type;
            
            // Check if the provided coordinates match the TimeFrame's coordinate type
            if (!std::holds_alternative<FrameCoordType>(start_coord) || 
                !std::holds_alternative<FrameCoordType>(end_coord)) {
                throw std::runtime_error("Coordinate type mismatch with TimeFrameV2. Expected: " + 
                                       getCoordinateType());
            }

            // Extract the correct coordinate values
            auto start_typed = std::get<FrameCoordType>(start_coord);
            auto end_typed = std::get<FrameCoordType>(end_coord);

            std::vector<float> result;

            // Get the range of indices for the coordinate range
            TimeFrameIndex start_idx = timeframe_ptr->getIndexAtTime(start_typed);
            TimeFrameIndex end_idx = timeframe_ptr->getIndexAtTime(end_typed);

            if (start_idx > end_idx) std::swap(start_idx, end_idx);

            result.reserve(static_cast<size_t>((end_idx - start_idx).getValue() + 1));

            for (TimeFrameIndex idx = start_idx; idx <= end_idx; ++idx) {
                if (idx.getValue() >= 0 && static_cast<size_t>(idx.getValue()) < _data.size()) {
                    result.push_back(_data[static_cast<size_t>(idx.getValue())]);
                }
            }

            return result;
        }, _timeframe_v2.value());
    }

    /**
    * @brief Get data values and their coordinates in a time range using any coordinate type
    *
    * Similar to getDataInCoordinateRange but returns both values and coordinates.
    * Uses TimeCoordinate variant to handle any coordinate type at runtime.
    *
    * @param start_coord Start coordinate (any coordinate type)
    * @param end_coord End coordinate (any coordinate type)
    * @return Pair of coordinate vector and value vector
    */
    std::pair<std::vector<TimeCoordinate>, std::vector<float>> getDataAndCoordsInRange(
            TimeCoordinate start_coord, TimeCoordinate end_coord) const {
        if (!_timeframe_v2.has_value()) {
            throw std::runtime_error("No TimeFrameV2 associated with this AnalogTimeSeries");
        }

        return std::visit([&](auto const & timeframe_ptr) -> std::pair<std::vector<TimeCoordinate>, std::vector<float>> {
            using FrameType = std::decay_t<decltype(*timeframe_ptr)>;
            using FrameCoordType = typename FrameType::coordinate_type;
            
            // Check coordinate type compatibility
            if (!std::holds_alternative<FrameCoordType>(start_coord) || 
                !std::holds_alternative<FrameCoordType>(end_coord)) {
                throw std::runtime_error("Coordinate type mismatch with TimeFrameV2. Expected: " + 
                                       getCoordinateType());
            }

            auto start_typed = std::get<FrameCoordType>(start_coord);
            auto end_typed = std::get<FrameCoordType>(end_coord);

            std::vector<TimeCoordinate> coords;
            std::vector<float> values;

            // Get the range of indices for the coordinate range
            TimeFrameIndex start_idx = timeframe_ptr->getIndexAtTime(start_typed);
            TimeFrameIndex end_idx = timeframe_ptr->getIndexAtTime(end_typed);

            if (start_idx > end_idx) std::swap(start_idx, end_idx);

            coords.reserve(static_cast<size_t>((end_idx - start_idx).getValue() + 1));
            values.reserve(static_cast<size_t>((end_idx - start_idx).getValue() + 1));

            for (TimeFrameIndex idx = start_idx; idx <= end_idx; ++idx) {
                if (idx.getValue() >= 0 && static_cast<size_t>(idx.getValue()) < _data.size()) {
                    // Store coordinate as TimeCoordinate variant
                    coords.push_back(TimeCoordinate{timeframe_ptr->getTimeAtIndex(idx)});
                    values.push_back(_data[static_cast<size_t>(idx.getValue())]);
                }
            }

            return {coords, values};
        }, _timeframe_v2.value());
    }

    /**
    * @brief Strongly-typed coordinate range query (compile-time type checking)
    *
    * This method provides compile-time type safety for coordinate queries.
    * Use this when you know the exact coordinate type at compile time.
    *
    * @tparam CoordinateType The expected coordinate type
    * @param start_coord Start coordinate 
    * @param end_coord End coordinate
    * @return Vector of data values within the specified coordinate range
    * @throws std::runtime_error if no TimeFrameV2 is associated or types don't match
    */
    template<typename CoordinateType>
    std::vector<float> getDataInTypedCoordinateRange(CoordinateType start_coord, CoordinateType end_coord) const {
        if (!_timeframe_v2.has_value()) {
            throw std::runtime_error("No TimeFrameV2 associated with this AnalogTimeSeries");
        }

        return std::visit([&](auto const & timeframe_ptr) -> std::vector<float> {
            using FrameType = std::decay_t<decltype(*timeframe_ptr)>;
            using FrameCoordType = typename FrameType::coordinate_type;
            
            if constexpr (std::is_same_v<FrameCoordType, CoordinateType>) {
                std::vector<float> result;

                // Get the range of indices for the coordinate range
                int64_t start_idx = timeframe_ptr->getIndexAtTime(start_coord);
                int64_t end_idx = timeframe_ptr->getIndexAtTime(end_coord);

                if (start_idx > end_idx) std::swap(start_idx, end_idx);

                result.reserve(static_cast<size_t>(end_idx - start_idx + 1));

                for (int64_t idx = start_idx; idx <= end_idx; ++idx) {
                    if (idx >= 0 && static_cast<size_t>(idx) < _data.size()) {
                        result.push_back(_data[static_cast<size_t>(idx)]);
                    }
                }

                return result;
            } else {
                throw std::runtime_error("Coordinate type mismatch with TimeFrameV2. Expected: " + 
                                       getCoordinateType() + ", Got: " + typeid(CoordinateType).name());
            }
        }, _timeframe_v2.value());
    }

    /**
    * @brief Strongly-typed coordinate and value range query
    *
    * @tparam CoordinateType The expected coordinate type
    * @param start_coord Start coordinate
    * @param end_coord End coordinate
    * @return Pair of coordinate vector and value vector
    */
    template<typename CoordinateType>
    std::pair<std::vector<CoordinateType>, std::vector<float>> getDataAndTypedCoordsInRange(
            CoordinateType start_coord, CoordinateType end_coord) const {
        if (!_timeframe_v2.has_value()) {
            throw std::runtime_error("No TimeFrameV2 associated with this AnalogTimeSeries");
        }

        return std::visit([&](auto const & timeframe_ptr) -> std::pair<std::vector<CoordinateType>, std::vector<float>> {
            using FrameType = std::decay_t<decltype(*timeframe_ptr)>;
            using FrameCoordType = typename FrameType::coordinate_type;
            
            if constexpr (std::is_same_v<FrameCoordType, CoordinateType>) {
                std::vector<CoordinateType> coords;
                std::vector<float> values;

                // Get the range of indices for the coordinate range
                int64_t start_idx = timeframe_ptr->getIndexAtTime(start_coord);
                int64_t end_idx = timeframe_ptr->getIndexAtTime(end_coord);

                if (start_idx > end_idx) std::swap(start_idx, end_idx);

                coords.reserve(static_cast<size_t>(end_idx - start_idx + 1));
                values.reserve(static_cast<size_t>(end_idx - start_idx + 1));

                for (int64_t idx = start_idx; idx <= end_idx; ++idx) {
                    if (idx >= 0 && static_cast<size_t>(idx) < _data.size()) {
                        coords.push_back(timeframe_ptr->getTimeAtIndex(idx));
                        values.push_back(_data[static_cast<size_t>(idx)]);
                    }
                }

                return {coords, values};
            } else {
                throw std::runtime_error("Coordinate type mismatch with TimeFrameV2. Expected: " + 
                                       getCoordinateType() + ", Got: " + typeid(CoordinateType).name());
            }
        }, _timeframe_v2.value());
    }

    // ========== Time Storage Optimization ==========
    
    /**
     * @brief Dense time representation for regularly sampled data
     * 
     * More memory efficient than storing every index individually.
     * Represents: start_index, start_index+1, start_index+2, ..., start_index+count-1
     */
    struct DenseTimeRange {
        size_t start_index;
        size_t count;
        
        DenseTimeRange(size_t start, size_t num_samples) 
            : start_index(start), count(num_samples) {}
            
        [[nodiscard]] size_t getTimeAtIndex(size_t i) const {
            if (i >= count) {
                throw std::out_of_range("Index out of range for DenseTimeRange");
            }
            return start_index + i;
        }
        
        [[nodiscard]] size_t size() const { return count; }
    };
    
    /**
     * @brief Sparse time representation for irregularly sampled data
     * 
     * Stores explicit time indices for each sample.
     */
    struct SparseTimeIndices {
        std::vector<size_t> indices;
        
        explicit SparseTimeIndices(std::vector<size_t> time_indices) 
            : indices(std::move(time_indices)) {}
            
        [[nodiscard]] size_t getTimeAtIndex(size_t i) const {
            if (i >= indices.size()) {
                throw std::out_of_range("Index out of range for SparseTimeIndices");
            }
            return indices[i];
        }

        [[nodiscard]] size_t size() const { return indices.size(); }
    };
    
    using TimeStorage = std::variant<DenseTimeRange, SparseTimeIndices>;

    /**
    * @brief Get a const reference to the time storage variant (DenseTimeRange or SparseTimeIndices)
    *
    * This allows efficient access to the underlying time mapping without copying.
    *
    * @return const TimeStorage& (std::variant<DenseTimeRange, SparseTimeIndices>)
    */
    const TimeStorage& getTimeStorage() const noexcept { return _time_storage; }

protected:
private:
    std::vector<float> _data;
    TimeStorage _time_storage;

    // New TimeFrameV2 support
    std::optional<AnyTimeFrame> _timeframe_v2;

    void setData(std::vector<float> analog_vector);
    void setData(std::vector<float> analog_vector, std::vector<size_t> time_vector);
    void setData(std::map<int, float> analog_map);
};

#endif// ANALOG_TIME_SERIES_HPP