#ifndef MOCK_DATA_TYPES_HPP
#define MOCK_DATA_TYPES_HPP

/**
 * @file MockDataTypes.hpp
 * @brief Test fixture types for storage unit tests
 * 
 * Provides minimal data types designed for testing storage operations in isolation.
 * These types allow testing storage logic without the complexity of real data types
 * like Mask2D or Line2D.
 */

#include <stdexcept>
#include <string>
#include <vector>

namespace TestFixtures {

/**
 * @brief Minimal data type for testing storage operations
 * 
 * Deliberately simple to isolate storage logic from data type complexity.
 * Has value semantics with default copy/move.
 */
struct SimpleData {
    int id = 0;
    float value = 0.0f;
    
    SimpleData() = default;
    SimpleData(int i, float v) : id(i), value(v) {}
    
    bool operator==(SimpleData const&) const = default;
};

/**
 * @brief Data type with expensive copy to test move semantics
 * 
 * Contains a large buffer and tracks copy/move operations
 * to verify that storage operations are efficient.
 */
struct HeavyData {
    std::vector<float> buffer;
    
    // Track operations for testing - must be reset between tests
    inline static int copy_count = 0;
    inline static int move_count = 0;
    
    /**
     * @brief Reset copy/move counters (call at start of each test)
     */
    static void resetCounters() { 
        copy_count = 0; 
        move_count = 0; 
    }
    
    HeavyData() = default;
    
    /**
     * @brief Construct with buffer of given size filled with 1.0f
     */
    explicit HeavyData(size_t size) : buffer(size, 1.0f) {}
    
    HeavyData(HeavyData const& other) : buffer(other.buffer) { 
        ++copy_count; 
    }
    
    HeavyData(HeavyData&& other) noexcept : buffer(std::move(other.buffer)) { 
        ++move_count; 
    }
    
    HeavyData& operator=(HeavyData const& other) { 
        buffer = other.buffer; 
        ++copy_count; 
        return *this; 
    }
    
    HeavyData& operator=(HeavyData&& other) noexcept { 
        buffer = std::move(other.buffer); 
        ++move_count; 
        return *this; 
    }
    
    bool operator==(HeavyData const&) const = default;
};

/**
 * @brief Data type that throws on copy (to verify no-copy paths)
 * 
 * Used to ensure that storage operations that claim to avoid
 * copying actually do so - will throw if an unexpected copy occurs.
 */
struct NoCopyData {
    int value = 0;
    
    NoCopyData() = default;
    explicit NoCopyData(int v) : value(v) {}
    
    NoCopyData(NoCopyData const&) { 
        throw std::runtime_error("Unexpected copy of NoCopyData!"); 
    }
    
    NoCopyData(NoCopyData&&) noexcept = default;
    
    NoCopyData& operator=(NoCopyData const&) { 
        throw std::runtime_error("Unexpected copy assignment of NoCopyData!"); 
    }
    
    NoCopyData& operator=(NoCopyData&&) noexcept = default;
    
    // Note: No operator== since copy throws
    bool equals(NoCopyData const& other) const {
        return value == other.value;
    }
};

/**
 * @brief Data type with a tag for tracking identity through storage
 * 
 * Useful for verifying that the correct data is returned from storage
 * without relying on equality comparisons.
 */
struct TaggedData {
    std::string tag;
    int sequence = 0;
    
    TaggedData() = default;
    TaggedData(std::string t, int seq) : tag(std::move(t)), sequence(seq) {}
    
    bool operator==(TaggedData const&) const = default;
};

} // namespace TestFixtures

#endif // MOCK_DATA_TYPES_HPP
