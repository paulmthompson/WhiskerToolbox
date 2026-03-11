/**
 * @file LazyAnalogDataStorage.hpp
 * @brief Lazy view-based analog data storage backend.
 */

#ifndef LAZY_ANALOG_DATA_STORAGE_HPP
#define LAZY_ANALOG_DATA_STORAGE_HPP

#include "AnalogDataStorageBase.hpp"

#include <ranges>
#include <span>

/**
 * @brief Lazy view-based analog data storage.
 *
 * Stores a computation pipeline as a random-access view that transforms data
 * on-demand. Enables efficient composition of transforms without materializing
 * intermediate results. Works with any random-access range that yields values
 * compatible with `AnalogTimeSeries::TimeValuePoint` or `(TimeFrameIndex, float)`
 * pairs (the value accessor is inferred).
 *
 * @tparam ViewType Type of the random-access range view.
 */
template<typename ViewType>
class LazyViewStorage : public AnalogDataStorageBase<LazyViewStorage<ViewType>> {
public:
    /**
     * @brief Construct lazy storage from a random-access view.
     *
     * @param view Random-access range view (must support operator[]).
     * @param num_samples Number of samples (must match view size).
     *
     * @throws std::invalid_argument if view is not random-access.
     */
    explicit LazyViewStorage(ViewType view, std::size_t num_samples)
        : _view(std::move(view)),
          _num_samples(num_samples) {
        static_assert(std::ranges::random_access_range<ViewType>,
                      "LazyViewStorage requires random access range");
    }

    ~LazyViewStorage() = default;

    // CRTP implementation methods

    [[nodiscard]] float getValueAtImpl(std::size_t index) const {
        auto element = _view[index];

        // Support both TimeValuePoint (with value() method) and std::pair<TimeFrameIndex, float>
        if constexpr (requires { element.value(); }) {
            return element.value();// TimeValuePoint
        } else {
            return element.second;// std::pair
        }
    }

    [[nodiscard]] std::size_t sizeImpl() const {
        return _num_samples;
    }

    [[nodiscard]] std::span<float const> getSpanImpl() const {
        // Lazy transforms are never contiguous in memory as float.
        return {};
    }

    [[nodiscard]] std::span<float const> getSpanRangeImpl(std::size_t /*start*/, std::size_t /*end*/) const {
        // Non-contiguous storage cannot provide spans.
        return {};
    }

    [[nodiscard]] bool isContiguousImpl() const {
        return false;
    }

    [[nodiscard]] AnalogStorageType getStorageTypeImpl() const {
        return AnalogStorageType::LazyView;
    }

    [[nodiscard]] AnalogDataStorageCache tryGetCacheImpl() const {
        return AnalogDataStorageCache{};
    }

    /**
     * @brief Get reference to underlying view (for advanced use).
     */
    [[nodiscard]] ViewType const & getView() const {
        return _view;
    }

private:
    ViewType _view;
    std::size_t _num_samples;
};

#endif// LAZY_ANALOG_DATA_STORAGE_HPP

