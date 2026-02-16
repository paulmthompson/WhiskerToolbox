#ifndef RAGGED_TIME_SERIES_TEST_TRAITS_HPP
#define RAGGED_TIME_SERIES_TEST_TRAITS_HPP

#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "builders/LineDataBuilder.hpp"
#include "builders/MaskDataBuilder.hpp"

#include <memory>
#include <vector>

/**
 * @brief Traits class for generic RaggedTimeSeries tests.
 *
 * Provides type-specific data generation logic for template tests.
 * Specializations must implement:
 * - DataType: The RaggedTimeSeries derived class (e.g. LineData)
 * - ElementType: The data element type (e.g. Line2D)
 * - createSingleItem(): Returns a valid element
 * - createAnotherItem(): Returns a different valid element
 * - createDataWithItems(times): Returns a shared_ptr to DataType populated at given times
 */
template <typename T>
struct RaggedDataTestTraits;

/**
 * @brief Specialization for LineData
 */
template <>
struct RaggedDataTestTraits<LineData> {
    using DataType = LineData;
    using ElementType = Line2D;

    static ElementType createSingleItem() {
        return line_shapes::horizontal(0, 10, 5);
    }

    static ElementType createAnotherItem() {
        return line_shapes::vertical(5, 0, 10);
    }

    static std::shared_ptr<DataType> createDataWithItems(std::vector<int> const& times) {
        LineDataBuilder builder;
        for (int t : times) {
            builder.atTime(t, createSingleItem());
        }
        return builder.build();
    }

    static bool equals(ElementType const& a, ElementType const& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (!(a[i] == b[i])) return false;
        }
        return true;
    }
};

/**
 * @brief Specialization for MaskData
 */
template <>
struct RaggedDataTestTraits<MaskData> {
    using DataType = MaskData;
    using ElementType = Mask2D;

    static ElementType createSingleItem() {
        return mask_shapes::box(0, 0, 10, 10);
    }

    static ElementType createAnotherItem() {
        return mask_shapes::circle(50, 50, 20);
    }

    static std::shared_ptr<DataType> createDataWithItems(std::vector<int> const& times) {
        MaskDataBuilder builder;
        for (int t : times) {
            builder.atTime(t, createSingleItem());
        }
        return builder.build();
    }

    static bool equals(ElementType const& a, ElementType const& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (!(a[i] == b[i])) return false;
        }
        return true;
    }
};

#endif // RAGGED_TIME_SERIES_TEST_TRAITS_HPP
