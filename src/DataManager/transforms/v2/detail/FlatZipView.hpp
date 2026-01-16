#pragma once

#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace WhiskerToolbox::Transforms::V2 {

/**
 * @brief Type trait to extract the time from a range element
 * 
 * Works with:
 * - pair<TimeFrameIndex, T> (from elements())
 * - tuple<TimeFrameIndex, EntityId, T> (from flattened_data())
 */
template<typename T>
struct TimeExtractor {
    static TimeFrameIndex get(T const & elem) {
        if constexpr (requires { elem.first; }) {
            // pair-like (from elements())
            return elem.first;
        } else if constexpr (requires { std::get<0>(elem); }) {
            // tuple-like (from flattened_data())
            return std::get<0>(elem);
        } else {
            static_assert(sizeof(T) == 0, "Element must be pair or tuple with TimeFrameIndex as first element");
        }
    }
};

/**
 * @brief Type trait to extract the data from a range element
 * 
 * Works with:
 * - pair<TimeFrameIndex, DataEntry<T>> -> returns DataEntry<T>::data
 * - tuple<TimeFrameIndex, EntityId, cref<T>> -> returns T
 */
template<typename T>
struct DataExtractor {
    static auto const & get(T const & elem) {
        if constexpr (requires { elem.second.data; }) {
            // pair with DataEntry (from elements())
            return elem.second.data;
        } else if constexpr (requires { elem.second; }) {
            // pair with raw data
            return elem.second;
        } else if constexpr (requires { std::get<2>(elem).get(); }) {
            // tuple with cref (from flattened_data())
            return std::get<2>(elem).get();
        } else if constexpr (requires { std::get<1>(elem); }) {
            // tuple with 2 elements
            return std::get<1>(elem);
        } else {
            static_assert(sizeof(T) == 0, "Cannot extract data from element type");
        }
    }

    using type = std::remove_cvref_t<decltype(get(std::declval<T>()))>;
};

/**
 * @brief A view that zips two time-ordered ranges with time synchronization and broadcasting
 * 
 * Works with ranges from RaggedTimeSeries::elements() or flattened_data().
 * Handles three cases:
 * 1. 1:1 matching: Both ranges have the same number of entries at each time
 * 2. Broadcast left: Left has 1 entry at time, right has N entries - broadcast left to all right
 * 3. Broadcast right: Right has 1 entry at time, left has N entries - broadcast right to all left
 * 
 * Skips times that only exist in one range.
 * 
 * Usage:
 *   // Using elements() from RaggedTimeSeries
 *   for (auto [time, line, point] : FlatZipView(lines->elements(), points->elements())) {
 *       // process line and point at time
 *   }
 * 
 *   // Can also create from materialized vectors
 *   auto line_elements = lines->elements() | std::ranges::to<std::vector>();
 *   auto point_elements = points->elements() | std::ranges::to<std::vector>();
 *   for (auto [time, line, point] : FlatZipView(line_elements, point_elements)) {
 *       // ...
 *   }
 */
template<typename Range1, typename Range2>
class FlatZipView {
public:
    using Elem1 = std::ranges::range_value_t<Range1>;
    using Elem2 = std::ranges::range_value_t<Range2>;
    using Data1 = typename DataExtractor<Elem1>::type;
    using Data2 = typename DataExtractor<Elem2>::type;
    using value_type = std::tuple<TimeFrameIndex, Data1 const &, Data2 const &>;

    /**
     * @brief Construct from two ranges (materializes them into vectors)
     * 
     * The ranges are materialized to allow random access during iteration.
     * This works with any input range, including lazy views from elements().
     */
    template<typename R1, typename R2>
    FlatZipView(R1 && range1, R2 && range2)
        : data1_(materialize(std::forward<R1>(range1))),
          data2_(materialize(std::forward<R2>(range2))) {
    }

    class Iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = std::tuple<TimeFrameIndex, Data1 const &, Data2 const &>;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = value_type;

        Iterator() = default;

        Iterator(std::vector<Elem1> const * data1, std::vector<Elem2> const * data2, bool is_end = false)
            : data1_(data1),
              data2_(data2) {
            if (is_end || data1_->empty() || data2_->empty()) {
                idx1_ = data1_->size();
                idx2_ = data2_->size();
            } else {
                synchronize();
                if (idx1_ < data1_->size() && idx2_ < data2_->size()) {
                    setupCurrentTime();
                }
            }
        }

        reference operator*() const {
            if (mode_ == Mode::OneToOne) {
                return {current_time_,
                        DataExtractor<Elem1>::get((*data1_)[idx1_ + inner_idx_]),
                        DataExtractor<Elem2>::get((*data2_)[idx2_ + inner_idx_])};
            } else if (mode_ == Mode::BroadcastLeft) {
                return {current_time_,
                        DataExtractor<Elem1>::get((*data1_)[idx1_]),
                        DataExtractor<Elem2>::get((*data2_)[idx2_ + inner_idx_])};
            } else {// BroadcastRight
                return {current_time_,
                        DataExtractor<Elem1>::get((*data1_)[idx1_ + inner_idx_]),
                        DataExtractor<Elem2>::get((*data2_)[idx2_])};
            }
        }

        Iterator & operator++() {
            ++inner_idx_;
            if (inner_idx_ >= current_count_) {
                // Move to next time range
                idx1_ += count1_;
                idx2_ += count2_;
                inner_idx_ = 0;

                if (idx1_ < data1_->size() && idx2_ < data2_->size()) {
                    synchronize();
                    if (idx1_ < data1_->size() && idx2_ < data2_->size()) {
                        setupCurrentTime();
                    }
                }
            }
            return *this;
        }

        Iterator operator++(int) {
            Iterator temp = *this;
            ++(*this);
            return temp;
        }

        bool operator==(Iterator const & other) const {
            bool is_end = (idx1_ >= data1_->size() || idx2_ >= data2_->size());
            bool other_is_end = (other.idx1_ >= other.data1_->size() || other.idx2_ >= other.data2_->size());
            return is_end == other_is_end;
        }

        bool operator!=(Iterator const & other) const {
            return !(*this == other);
        }

    private:
        enum class Mode { OneToOne,
                          BroadcastLeft,
                          BroadcastRight };

        TimeFrameIndex getTime1(size_t idx) const {
            return TimeExtractor<Elem1>::get((*data1_)[idx]);
        }

        TimeFrameIndex getTime2(size_t idx) const {
            return TimeExtractor<Elem2>::get((*data2_)[idx]);
        }

        void synchronize() {
            while (idx1_ < data1_->size() && idx2_ < data2_->size()) {
                auto time1 = getTime1(idx1_);
                auto time2 = getTime2(idx2_);

                if (time1 < time2) {
                    // Skip all entries at this time in range 1
                    while (idx1_ < data1_->size() && getTime1(idx1_) == time1) {
                        ++idx1_;
                    }
                } else if (time2 < time1) {
                    // Skip all entries at this time in range 2
                    while (idx2_ < data2_->size() && getTime2(idx2_) == time2) {
                        ++idx2_;
                    }
                } else {
                    // Times match
                    return;
                }
            }
        }

        void setupCurrentTime() {
            current_time_ = getTime1(idx1_);

            // Count entries at this time in both ranges
            count1_ = 0;
            for (size_t i = idx1_; i < data1_->size() && getTime1(i) == current_time_; ++i) {
                ++count1_;
            }

            count2_ = 0;
            for (size_t i = idx2_; i < data2_->size() && getTime2(i) == current_time_; ++i) {
                ++count2_;
            }

            // Determine mode
            if (count1_ == count2_) {
                mode_ = Mode::OneToOne;
                current_count_ = count1_;
            } else if (count1_ == 1) {
                mode_ = Mode::BroadcastLeft;
                current_count_ = count2_;
            } else if (count2_ == 1) {
                mode_ = Mode::BroadcastRight;
                current_count_ = count1_;
            } else {
                throw std::runtime_error(
                        "FlatZipView: Shape mismatch at time " + std::to_string(current_time_.getValue()) +
                        ". Left count: " + std::to_string(count1_) +
                        ", Right count: " + std::to_string(count2_) +
                        ". Broadcasting requires equal counts or one side to have count 1.");
            }

            inner_idx_ = 0;
        }

        std::vector<Elem1> const * data1_ = nullptr;
        std::vector<Elem2> const * data2_ = nullptr;

        size_t idx1_ = 0;
        size_t idx2_ = 0;
        size_t inner_idx_ = 0;
        size_t count1_ = 0;
        size_t count2_ = 0;
        size_t current_count_ = 0;
        TimeFrameIndex current_time_{0};
        Mode mode_ = Mode::OneToOne;
    };

    auto begin() const {
        return Iterator(&data1_, &data2_, false);
    }

    auto end() const {
        return Iterator(&data1_, &data2_, true);
    }

    [[nodiscard]] bool empty() const {
        return data1_.empty() || data2_.empty();
    }

private:
    template<typename R>
    static auto materialize(R && range) {
        using ElemType = std::ranges::range_value_t<std::remove_cvref_t<R>>;
        if constexpr (std::is_same_v<std::remove_cvref_t<R>, std::vector<ElemType>>) {
            // Already a vector, just forward/copy
            return std::forward<R>(range);
        } else {
            // Materialize the range into a vector
            std::vector<ElemType> result;
            for (auto && elem: range) {
                result.push_back(std::forward<decltype(elem)>(elem));
            }
            return result;
        }
    }

    std::vector<Elem1> data1_;
    std::vector<Elem2> data2_;
};

// Deduction guide for ranges
template<typename R1, typename R2>
FlatZipView(R1 &&, R2 &&) -> FlatZipView<
        std::vector<std::ranges::range_value_t<std::remove_cvref_t<R1>>>,
        std::vector<std::ranges::range_value_t<std::remove_cvref_t<R2>>>>;

/**
 * @brief Helper function to create a FlatZipView from two RaggedTimeSeries
 * 
 * Usage:
 *   auto zip = makeZipView(*lines, *points);
 *   for (auto [time, line, point] : zip) { ... }
 */
template<typename TS1, typename TS2>
    requires requires(TS1 const & ts1, TS2 const & ts2) {
        ts1.elements();
        ts2.elements();
    }
auto makeZipView(TS1 const & ts1, TS2 const & ts2) {
    return FlatZipView(ts1.elements(), ts2.elements());
}

}// namespace WhiskerToolbox::Transforms::V2
