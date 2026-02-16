#ifndef RAGGED_TEST_TRAITS_HPP
#define RAGGED_TEST_TRAITS_HPP

/**
 * @file RaggedTestTraits.hpp
 * @brief Type-aware test traits for RaggedTimeSeries-derived data types
 *
 * Each specialization provides:
 *  - DataType          — the concrete RaggedTimeSeries-derived class (LineData, MaskData, PointData)
 *  - sample1/2/3()     — three distinct sample data objects (Line2D, Mask2D, Point2D<float>)
 *  - add(data, idx, s) — adds a sample to `data` at TimeFrameIndex `idx`
 *  - type_tag          — Catch2 tag string for filtering ("[line]", "[mask]", "[point]")
 *
 * This allows TEMPLATE_TEST_CASE to exercise every shared RaggedTimeSeries
 * operation against all concrete types with zero per-type boilerplate in the
 * test body.
 */

#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "Observer/Observer_Data.hpp"

// --------------------------------------------------------------------------
// Primary template (unspecialized — intentionally incomplete)
// --------------------------------------------------------------------------
template<typename DataType>
struct RaggedTestTraits;

// --------------------------------------------------------------------------
// LineData
// --------------------------------------------------------------------------
template<>
struct RaggedTestTraits<LineData> {
    using DataType = LineData;
    using ElementType = Line2D;

    static constexpr char const * type_tag = "[line]";

    static ElementType sample1() {
        return Line2D(std::vector<float>{1.0f, 2.0f, 3.0f},
                      std::vector<float>{1.0f, 2.0f, 3.0f});
    }
    static ElementType sample2() {
        return Line2D(std::vector<float>{4.0f, 5.0f, 6.0f},
                      std::vector<float>{4.0f, 5.0f, 6.0f});
    }
    static ElementType sample3() {
        return Line2D(std::vector<float>{7.0f, 8.0f, 9.0f},
                      std::vector<float>{7.0f, 8.0f, 9.0f});
    }

    static void add(DataType & data, TimeFrameIndex idx, ElementType const & elem,
                    NotifyObservers notify = NotifyObservers::No) {
        data.addAtTime(idx, elem, notify);
    }

    /// Verify the first coordinate of an element (used for identity checks)
    static bool starts_with(ElementType const & elem, float expected_x) {
        return !elem.empty() && elem[0].x == expected_x;
    }
};

// --------------------------------------------------------------------------
// MaskData
// --------------------------------------------------------------------------
template<>
struct RaggedTestTraits<MaskData> {
    using DataType = MaskData;
    using ElementType = Mask2D;

    static constexpr char const * type_tag = "[mask]";

    static ElementType sample1() {
        return Mask2D(std::vector<uint32_t>{1, 2, 3, 1},
                      std::vector<uint32_t>{1, 1, 2, 2});
    }
    static ElementType sample2() {
        return Mask2D(std::vector<uint32_t>{4, 5, 6, 4},
                      std::vector<uint32_t>{3, 3, 4, 4});
    }
    static ElementType sample3() {
        return Mask2D(std::vector<uint32_t>{10, 11, 12},
                      std::vector<uint32_t>{10, 10, 11});
    }

    static void add(DataType & data, TimeFrameIndex idx, ElementType const & elem,
                    NotifyObservers notify = NotifyObservers::No) {
        data.addAtTime(idx, elem, notify);
    }

    static bool starts_with(ElementType const & elem, float expected_x) {
        return !elem.empty() && static_cast<float>(elem[0].x) == expected_x;
    }
};

// --------------------------------------------------------------------------
// PointData
// --------------------------------------------------------------------------
template<>
struct RaggedTestTraits<PointData> {
    using DataType = PointData;
    using ElementType = Point2D<float>;

    static constexpr char const * type_tag = "[point]";

    static ElementType sample1() { return Point2D<float>{1.0f, 2.0f}; }
    static ElementType sample2() { return Point2D<float>{4.0f, 5.0f}; }
    static ElementType sample3() { return Point2D<float>{7.0f, 8.0f}; }

    static void add(DataType & data, TimeFrameIndex idx, ElementType const & elem,
                    NotifyObservers notify = NotifyObservers::No) {
        data.addAtTime(idx, elem, notify);
    }

    static bool starts_with(ElementType const & elem, float expected_x) {
        return elem.x == expected_x;
    }
};

#endif // RAGGED_TEST_TRAITS_HPP
