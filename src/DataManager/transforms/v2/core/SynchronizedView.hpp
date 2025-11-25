#pragma once

#include "TimeFrame/TimeFrame.hpp"
#include <ranges>
#include <iterator>
#include <tuple>

namespace WhiskerToolbox::DataManager::Transforms::V2 {

/**
 * @brief A view that synchronizes two time-ordered ranges
 * 
 * Takes two ranges that yield (TimeFrameIndex, Value) pairs and produces
 * a sequence of (TimeFrameIndex, Value1, Value2) for matching times.
 * Assumes both input ranges are strictly ordered by time.
 */
template<std::ranges::input_range R1, std::ranges::input_range R2>
class SynchronizedView : public std::ranges::view_interface<SynchronizedView<R1, R2>> {
public:
    SynchronizedView() = default;
    SynchronizedView(R1 base1, R2 base2) 
        : base1_(std::move(base1)), base2_(std::move(base2)) {}

    class Iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = std::tuple<TimeFrameIndex, 
                                    decltype(std::declval<std::ranges::range_value_t<R1>>().second),
                                    decltype(std::declval<std::ranges::range_value_t<R2>>().second)>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type;

        Iterator() = default;
        Iterator(std::ranges::iterator_t<R1> it1, std::ranges::sentinel_t<R1> end1,
                 std::ranges::iterator_t<R2> it2, std::ranges::sentinel_t<R2> end2)
            : it1_(it1), end1_(end1), it2_(it2), end2_(end2) {
            synchronize();
        }

        reference operator*() const {
            return {it1_->first, it1_->second, it2_->second};
        }

        Iterator& operator++() {
            ++it1_;
            ++it2_;
            synchronize();
            return *this;
        }

        Iterator operator++(int) {
            Iterator temp = *this;
            ++(*this);
            return temp;
        }

        bool operator==(Iterator const& other) const {
            // Only equality with end iterator matters for input ranges
            bool is_end = (it1_ == end1_) || (it2_ == end2_);
            bool other_is_end = (other.it1_ == other.end1_) || (other.it2_ == other.end2_);
            return is_end == other_is_end;
        }

        bool operator!=(Iterator const& other) const {
            return !(*this == other);
        }

    private:
        void synchronize() {
            while (it1_ != end1_ && it2_ != end2_) {
                if (it1_->first < it2_->first) {
                    ++it1_;
                } else if (it2_->first < it1_->first) {
                    ++it2_;
                } else {
                    // Times match
                    return;
                }
            }
        }

        std::ranges::iterator_t<R1> it1_;
        std::ranges::sentinel_t<R1> end1_;
        std::ranges::iterator_t<R2> it2_;
        std::ranges::sentinel_t<R2> end2_;
    };

    auto begin() {
        return Iterator(std::ranges::begin(base1_), std::ranges::end(base1_),
                       std::ranges::begin(base2_), std::ranges::end(base2_));
    }

    auto end() {
        return Iterator(std::ranges::end(base1_), std::ranges::end(base1_),
                       std::ranges::end(base2_), std::ranges::end(base2_));
    }

private:
    R1 base1_;
    R2 base2_;
};

template<typename R1, typename R2>
SynchronizedView(R1&&, R2&&) -> SynchronizedView<std::views::all_t<R1>, std::views::all_t<R2>>;

} // namespace WhiskerToolbox::DataManager::Transforms::V2
