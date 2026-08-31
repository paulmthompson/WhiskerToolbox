#ifndef NEURALYZER_V2_MULTICHANNEL_TEMPLATE_REALIGNER_HPP
#define NEURALYZER_V2_MULTICHANNEL_TEMPLATE_REALIGNER_HPP

/**
 * @file MultichannelTemplateRealigner.hpp
 * @brief High-performance modular engine for spatio-temporal template calculation and iterative waveform realignment.
 *
 * Implements Expectation-Maximization (EM) template realignment matching SpikeSorter:
 * 1. M-Step: Compute multichannel spatio-temporal cluster mean template T(c, τ)
 * 2. E-Step: Slide each event within [-W_search, +W_search] to maximize template goodness-of-fit / cross-correlation
 * 3. Iterate until shifts converge to 0.
 */

#include "CoreMath/sinc_interpolation.hpp"

#include <armadillo>

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Neuralyzer::Transforms::V2 {

/**
 * @brief Multichannel spatio-temporal waveform template container.
 */
struct MultichannelTemplate {
    std::vector<float> data{};      ///< Row-major flattened matrix [num_channels x num_points]
    size_t num_channels{0};         ///< Number of channels in template
    size_t num_points{0};           ///< Total time points per channel (w_pre + w_post + 1)
    int w_pre{0};                   ///< Pre-peak sample points
    int w_post{0};                  ///< Post-peak sample points

    [[nodiscard]] float const * channelData(size_t ch_idx) const noexcept {
        assert(ch_idx < num_channels && "channelData: ch_idx out of bounds");
        return data.data() + (ch_idx * num_points);
    }
};

/**
 * @brief Configuration parameters for template realignment.
 */
struct TemplateRealignerParams {
    int w_pre{10};                  ///< Pre-peak window in samples (e.g. 0.33 ms @ 30 kHz)
    int w_post{21};                 ///< Post-peak window in samples (e.g. 0.70 ms @ 30 kHz)
    int search_window{15};          ///< Maximum search range (+/- samples, e.g. 15 samples = +/- 0.5 ms @ 30 kHz)
    int max_iterations{5};          ///< Maximum Expectation-Maximization iterations
    int sinc_sub_divisions{8};      ///< Sub-divisions per sample for sub-sample search
    int sinc_kernel_half_width{8};  ///< Sinc interpolation kernel half width
};

/**
 * @brief Modular numerical engine for multichannel template calculation and alignment.
 */
class MultichannelTemplateRealigner {
public:
    /**
     * @brief Compute multichannel cluster template by averaging aligned waveforms.
     *
     * @pre !voltage_matrix.is_empty()
     * @pre !event_sample_indices.empty()
     * @pre !selected_channels.empty()
     * @param voltage_matrix Continuous multichannel voltage matrix (samples x channels)
     * @param event_sample_indices Discrete sample indices of events in this cluster
     * @param selected_channels Channel indices included in template
     * @param w_pre Pre-peak sample window
     * @param w_post Post-peak sample window
     * @return MultichannelTemplate matrix
     */
    [[nodiscard]] static MultichannelTemplate computeTemplate(
            arma::fmat const & voltage_matrix,
            std::span<int64_t const> event_sample_indices,
            std::span<int const> selected_channels,
            int w_pre,
            int w_post);

    /**
     * @brief Compute multichannel template cross-correlation / goodness-of-fit.
     *
     * @param voltage_matrix Continuous voltage matrix
     * @param event_sample_index Center sample index
     * @param shift_offset Shift offset in samples
     * @param selected_channels Channel indices
     * @param tmpl Multichannel template
     * @return Inner product sum V * T
     */
    [[nodiscard]] static float computeCrossCorrelation(
            arma::fmat const & voltage_matrix,
            int64_t event_sample_index,
            int shift_offset,
            std::span<int const> selected_channels,
            MultichannelTemplate const & tmpl) noexcept;

    /**
     * @brief Find optimal integer shift that maximizes multichannel goodness-of-fit against template.
     *
     * @param voltage_matrix Continuous voltage matrix
     * @param event_sample_index Center sample index
     * @param selected_channels Channel indices
     * @param tmpl Multichannel template
     * @param search_window Max search shift (+/- samples)
     * @return Optimal integer shift in [-search_window, +search_window]
     */
    [[nodiscard]] static int findOptimalDiscreteShift(
            arma::fmat const & voltage_matrix,
            int64_t event_sample_index,
            std::span<int const> selected_channels,
            MultichannelTemplate const & tmpl,
            int search_window) noexcept;

    /**
     * @brief Find optimal continuous sub-sample shift using sinc interpolation.
     *
     * @param voltage_matrix Continuous voltage matrix
     * @param event_sample_index Center sample index
     * @param selected_channels Channel indices
     * @param tmpl Multichannel template
     * @param search_window Max search shift (+/- samples)
     * @param sub_divisions Sub-sample steps per sample (e.g. 8 or 16)
     * @param kernel_half_width Sinc kernel half width
     * @return Continuous optimal shift in samples
     */
    [[nodiscard]] static double findOptimalSubSampleShift(
            arma::fmat const & voltage_matrix,
            int64_t event_sample_index,
            std::span<int const> selected_channels,
            MultichannelTemplate const & tmpl,
            int search_window,
            int sub_divisions = 8,
            int kernel_half_width = 8);

    /**
     * @brief Iteratively realign event timestamps to convergence using discrete shifts.
     *
     * Runs EM iterations: Template update -> Event shift -> Template update until shifts == 0.
     *
     * @param voltage_matrix Continuous voltage matrix
     * @param initial_event_sample_indices Initial detected sample indices
     * @param selected_channels Channel indices
     * @param params Realignment parameters
     * @return Vector of realigned discrete sample indices
     */
    [[nodiscard]] static std::vector<int64_t> realignEventsDiscrete(
            arma::fmat const & voltage_matrix,
            std::span<int64_t const> initial_event_sample_indices,
            std::span<int const> selected_channels,
            TemplateRealignerParams const & params);

    /**
     * @brief Compute continuous sub-sample alignment offsets for a set of events against their template.
     *
     * @param voltage_matrix Continuous voltage matrix
     * @param event_sample_indices Discrete event sample indices
     * @param selected_channels Channel indices
     * @param params Realignment parameters
     * @return Vector of continuous sub-sample shifts (one per event)
     */
    [[nodiscard]] static std::vector<double> computeSubSampleShifts(
            arma::fmat const & voltage_matrix,
            std::span<int64_t const> event_sample_indices,
            std::span<int const> selected_channels,
            TemplateRealignerParams const & params);
};

}// namespace Neuralyzer::Transforms::V2

#endif// NEURALYZER_V2_MULTICHANNEL_TEMPLATE_REALIGNER_HPP
