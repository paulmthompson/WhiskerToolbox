#pragma once

#include "SynchronizedView.hpp"
#include <ranges>
#include <iterator>
#include <tuple>
#include <stdexcept>
#include <string>
#include <span>

namespace WhiskerToolbox::Transforms::V2 {

/**
 * @brief A view that zips two ragged time series with broadcasting support
 * 
 * Takes two ranges that yield (TimeFrameIndex, Span1, Span2) triplets (typically from SynchronizedView)
 * and produces a flattened sequence of (TimeFrameIndex, Value1, Value2).
 * 
 * Supports broadcasting rules:
 * 1. If sizes match: 1:1 pairing
 * 2. If Span1 size is 1: Broadcast Span1[0] to all Span2 elements
 * 3. If Span2 size is 1: Broadcast Span2[0] to all Span1 elements
 * 4. Otherwise: Throws std::runtime_error
 */
template<std::ranges::input_range R1, std::ranges::input_range R2>
class RaggedZipView : public std::ranges::view_interface<RaggedZipView<R1, R2>> {
public:
    using SyncView = SynchronizedView<R1, R2>;
    
    RaggedZipView() = default;
    RaggedZipView(R1 base1, R2 base2) 
        : sync_view_(std::move(base1), std::move(base2)) {}

    class Iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        // Value type is tuple<Time, Ref1, Ref2>
        // We need to deduce the reference types from the spans
        using Span1 = std::tuple_element_t<1, std::ranges::range_value_t<SyncView>>;
        using Span2 = std::tuple_element_t<2, std::ranges::range_value_t<SyncView>>;
        using Ref1 = std::ranges::range_reference_t<Span1>;
        using Ref2 = std::ranges::range_reference_t<Span2>;
        
        using value_type = std::tuple<TimeFrameIndex, Ref1, Ref2>;
        using difference_type = std::ptrdiff_t;
        using pointer = void; // Input iterator doesn't need pointer
        using reference = value_type;

        Iterator() = default;
        Iterator(std::ranges::iterator_t<SyncView> it, std::ranges::sentinel_t<SyncView> end)
            : outer_it_(it), outer_end_(end) {
            if (outer_it_ != outer_end_) {
                setupCurrentStep();
            }
        }

        reference operator*() const {
            auto const& [time, span1, span2] = *outer_it_;
            
            if (mode_ == Mode::OneToOne) {
                return {time, span1[inner_index_], span2[inner_index_]};
            } else if (mode_ == Mode::BroadcastLeft) {
                return {time, span1[0], span2[inner_index_]};
            } else { // BroadcastRight
                return {time, span1[inner_index_], span2[0]};
            }
        }

        Iterator& operator++() {
            ++inner_index_;
            if (inner_index_ >= current_size_) {
                ++outer_it_;
                if (outer_it_ != outer_end_) {
                    setupCurrentStep();
                }
            }
            return *this;
        }

        Iterator operator++(int) {
            Iterator temp = *this;
            ++(*this);
            return temp;
        }

        bool operator==(Iterator const& other) const {
            // Only equality with end iterator matters
            bool is_end = (outer_it_ == outer_end_);
            bool other_is_end = (other.outer_it_ == other.outer_end_);
            return is_end == other_is_end;
        }

        bool operator!=(Iterator const& other) const {
            return !(*this == other);
        }

    private:
        enum class Mode { OneToOne, BroadcastLeft, BroadcastRight };

        void setupCurrentStep() {
            auto const& [time, span1, span2] = *outer_it_;
            size_t s1 = span1.size();
            size_t s2 = span2.size();

            inner_index_ = 0;

            if (s1 == s2) {
                mode_ = Mode::OneToOne;
                current_size_ = s1;
            } else if (s1 == 1) {
                mode_ = Mode::BroadcastLeft;
                current_size_ = s2;
            } else if (s2 == 1) {
                mode_ = Mode::BroadcastRight;
                current_size_ = s1;
            } else {
                throw std::runtime_error(
                    "RaggedZipView: Shape mismatch at time " + std::to_string(time.getValue()) + 
                    ". Left size: " + std::to_string(s1) + 
                    ", Right size: " + std::to_string(s2) + 
                    ". Broadcasting requires equal sizes or one side to have size 1.");
            }
            
            // Handle empty spans (skip this time step)
            if (current_size_ == 0) {
                ++outer_it_;
                if (outer_it_ != outer_end_) {
                    setupCurrentStep();
                }
            }
        }

        std::ranges::iterator_t<SyncView> outer_it_;
        std::ranges::sentinel_t<SyncView> outer_end_;
        size_t inner_index_ = 0;
        size_t current_size_ = 0;
        Mode mode_ = Mode::OneToOne;
    };

    auto begin() {
        return Iterator(sync_view_.begin(), sync_view_.end());
    }

    auto end() {
        return Iterator(sync_view_.end(), sync_view_.end());
    }

private:
    SyncView sync_view_;
};

template<typename R1, typename R2>
RaggedZipView(R1&&, R2&&) -> RaggedZipView<std::views::all_t<R1>, std::views::all_t<R2>>;

} // namespace WhiskerToolbox::DataManager::Transforms::V2
