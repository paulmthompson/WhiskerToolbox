#ifndef MASK_IDENTIFIER_HPP
#define MASK_IDENTIFIER_HPP

#include <cstdint>
#include <cstddef>

/**
 * @brief Identifier for a specific mask within the dataset
 */
struct MaskIdentifier {
    int64_t timeframe;
    size_t mask_index;

    MaskIdentifier() : timeframe(0), mask_index(0) {}
    
    MaskIdentifier(int64_t t, size_t idx) : timeframe(t), mask_index(idx) {}
    
    bool operator==(const MaskIdentifier& other) const {
        return timeframe == other.timeframe && mask_index == other.mask_index;
    }

    bool operator<(const MaskIdentifier& other) const {
        return (timeframe < other.timeframe) || 
               (timeframe == other.timeframe && mask_index < other.mask_index);
    }
};


#endif// MASK_IDENTIFIER_HPP