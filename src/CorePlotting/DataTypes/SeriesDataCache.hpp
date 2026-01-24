#ifndef COREPLOTTING_SERIESDATACACHE_HPP
#define COREPLOTTING_SERIESDATACACHE_HPP

namespace CorePlotting {

/**
 * @brief Cached statistical data for a series
 * 
 * Contains computed values that are expensive to calculate (e.g., mean,
 * standard deviation) but needed for display calculations (e.g., intrinsic
 * scaling). These are mutable caches that get invalidated when the
 * underlying data changes.
 * 
 * Separated from style configuration and layout results per the
 * CorePlotting architecture (see DESIGN.md, ROADMAP.md Phase 0).
 * 
 * @note All members are mutable to allow lazy computation even when
 *       accessed through const references.
 */
struct SeriesDataCache {
    mutable float cached_std_dev{0.0f};     ///< Cached standard deviation
    mutable bool std_dev_cache_valid{false}; ///< Is std_dev cache valid?
    mutable float cached_mean{0.0f};        ///< Cached mean value
    mutable bool mean_cache_valid{false};   ///< Is mean cache valid?
    mutable float intrinsic_scale{1.0f};    ///< Computed normalization scale (e.g., 1/(3*std_dev))

    /**
     * @brief Construct with invalid cache
     */
    SeriesDataCache() = default;

    /**
     * @brief Invalidate all cached values
     * 
     * Call this when the underlying data changes.
     */
    void invalidate() {
        std_dev_cache_valid = false;
        mean_cache_valid = false;
        intrinsic_scale = 1.0f;
    }

    /**
     * @brief Check if cache is fully valid
     * @return true if all cached values are valid
     */
    [[nodiscard]] bool is_valid() const {
        return std_dev_cache_valid && mean_cache_valid;
    }
};

} // namespace CorePlotting

#endif // COREPLOTTING_SERIESDATACACHE_HPP
