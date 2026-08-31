/**
 * @file MultichannelTemplateRealigner.cpp
 * @brief Implementation of MultichannelTemplateRealigner modular engine.
 */

#include "MultichannelTemplateRealigner.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

namespace Neuralyzer::Transforms::V2 {

MultichannelTemplate MultichannelTemplateRealigner::computeTemplate(
        arma::fmat const & voltage_matrix,
        std::span<int64_t const> event_sample_indices,
        std::span<int const> selected_channels,
        int w_pre,
        int w_post) {

    assert(!voltage_matrix.is_empty() && "computeTemplate: voltage_matrix is empty");
    assert(!selected_channels.empty() && "computeTemplate: selected_channels is empty");

    size_t const num_ch = selected_channels.size();
    size_t const num_pts = static_cast<size_t>(w_pre + w_post + 1);
    size_t const total_samples = voltage_matrix.n_rows;

    MultichannelTemplate tmpl;
    tmpl.num_channels = num_ch;
    tmpl.num_points = num_pts;
    tmpl.w_pre = w_pre;
    tmpl.w_post = w_post;
    tmpl.data.assign(num_ch * num_pts, 0.0f);

    if (event_sample_indices.empty()) {
        return tmpl;
    }

    size_t valid_event_count = 0;

    for (int64_t const t_event: event_sample_indices) {
        if (t_event < w_pre || t_event + w_post >= static_cast<int64_t>(total_samples)) {
            continue;// Skip edge events that cannot fit full template
        }
        ++valid_event_count;

        for (size_t c_idx = 0; c_idx < num_ch; ++c_idx) {
            int const ch = selected_channels[c_idx];
            float const * col = voltage_matrix.colptr(static_cast<size_t>(ch));
            float * tmpl_col = tmpl.data.data() + (c_idx * num_pts);

            for (int dt = -w_pre; dt <= w_post; ++dt) {
                size_t const pt_idx = static_cast<size_t>(dt + w_pre);
                tmpl_col[pt_idx] += col[t_event + dt];
            }
        }
    }

    if (valid_event_count > 0) {
        float const inv_count = 1.0f / static_cast<float>(valid_event_count);
        for (float & val: tmpl.data) {
            val *= inv_count;
        }
    }

    return tmpl;
}

float MultichannelTemplateRealigner::computeCrossCorrelation(
        arma::fmat const & voltage_matrix,
        int64_t event_sample_index,
        int shift_offset,
        std::span<int const> selected_channels,
        MultichannelTemplate const & tmpl) noexcept {

    int64_t const shifted_center = event_sample_index + shift_offset;
    int const w_pre = tmpl.w_pre;
    int const w_post = tmpl.w_post;
    size_t const num_ch = selected_channels.size();
    int64_t const total_samples = static_cast<int64_t>(voltage_matrix.n_rows);

    if (shifted_center < w_pre || shifted_center + w_post >= total_samples) {
        return -std::numeric_limits<float>::infinity();
    }

    double sum_dot = 0.0;

    for (size_t c_idx = 0; c_idx < num_ch; ++c_idx) {
        int const ch = selected_channels[c_idx];
        float const * col = voltage_matrix.colptr(static_cast<size_t>(ch));
        float const * tmpl_col = tmpl.channelData(c_idx);

        for (int dt = -w_pre; dt <= w_post; ++dt) {
            size_t const pt_idx = static_cast<size_t>(dt + w_pre);
            sum_dot += static_cast<double>(col[shifted_center + dt]) * static_cast<double>(tmpl_col[pt_idx]);
        }
    }

    return static_cast<float>(sum_dot);
}

int MultichannelTemplateRealigner::findOptimalDiscreteShift(
        arma::fmat const & voltage_matrix,
        int64_t event_sample_index,
        std::span<int const> selected_channels,
        MultichannelTemplate const & tmpl,
        int search_window) noexcept {

    int best_shift = 0;
    float max_score = -std::numeric_limits<float>::infinity();

    for (int shift = -search_window; shift <= search_window; ++shift) {
        float const score = computeCrossCorrelation(
                voltage_matrix, event_sample_index, shift, selected_channels, tmpl);
        if (score > max_score) {
            max_score = score;
            best_shift = shift;
        }
    }

    return best_shift;
}

double MultichannelTemplateRealigner::findOptimalSubSampleShift(
        arma::fmat const & voltage_matrix,
        int64_t event_sample_index,
        std::span<int const> selected_channels,
        MultichannelTemplate const & tmpl,
        int search_window,
        int sub_divisions,
        int kernel_half_width) {

    assert(sub_divisions >= 1 && "findOptimalSubSampleShift: sub_divisions must be >= 1");

    int const best_discrete = findOptimalDiscreteShift(
            voltage_matrix, event_sample_index, selected_channels, tmpl, search_window);

    // Fine continuous search within +/- 1.0 sample of best discrete shift
    double const step = 1.0 / static_cast<double>(sub_divisions);
    double const start_shift = static_cast<double>(best_discrete) - 1.0;
    double const end_shift = static_cast<double>(best_discrete) + 1.0;

    int const w_pre = tmpl.w_pre;
    int const w_post = tmpl.w_post;
    size_t const num_ch = selected_channels.size();
    int64_t const total_samples = static_cast<int64_t>(voltage_matrix.n_rows);

    double best_continuous_shift = static_cast<double>(best_discrete);
    double max_score = -std::numeric_limits<double>::infinity();

    for (double shift = start_shift; shift <= end_shift; shift += step) {
        double sum_dot = 0.0;
        bool valid = true;

        for (size_t c_idx = 0; c_idx < num_ch; ++c_idx) {
            int const ch = selected_channels[c_idx];
            float const * col = voltage_matrix.colptr(static_cast<size_t>(ch));
            std::span<float const> col_span(col, static_cast<size_t>(total_samples));
            float const * tmpl_col = tmpl.channelData(c_idx);

            for (int dt = -w_pre; dt <= w_post; ++dt) {
                double const sample_pos = static_cast<double>(event_sample_index + dt) + shift;
                if (sample_pos < 0.0 || sample_pos >= static_cast<double>(total_samples - 1)) {
                    valid = false;
                    break;
                }
                float const interp_val = CoreMath::SincInterpolator::interpolateAt(
                        col_span, sample_pos, kernel_half_width, CoreMath::SincWindowType::Hann);
                size_t const pt_idx = static_cast<size_t>(dt + w_pre);
                sum_dot += static_cast<double>(interp_val) * static_cast<double>(tmpl_col[pt_idx]);
            }
            if (!valid) break;
        }

        if (valid && sum_dot > max_score) {
            max_score = sum_dot;
            best_continuous_shift = shift;
        }
    }

    return best_continuous_shift;
}

std::vector<int64_t> MultichannelTemplateRealigner::realignEventsDiscrete(
        arma::fmat const & voltage_matrix,
        std::span<int64_t const> initial_event_sample_indices,
        std::span<int const> selected_channels,
        TemplateRealignerParams const & params) {

    if (initial_event_sample_indices.empty() || selected_channels.empty()) {
        return {initial_event_sample_indices.begin(), initial_event_sample_indices.end()};
    }

    std::vector<int64_t> current_times(
            initial_event_sample_indices.begin(), initial_event_sample_indices.end());

    int64_t const total_samples = static_cast<int64_t>(voltage_matrix.n_rows);

    for (int iter = 0; iter < params.max_iterations; ++iter) {
        // 1. Compute current cluster template
        auto const tmpl = computeTemplate(
                voltage_matrix, current_times, selected_channels, params.w_pre, params.w_post);

        // 2. Realign each event to the template
        bool any_shift = false;
        for (size_t i = 0; i < current_times.size(); ++i) {
            int const shift = findOptimalDiscreteShift(
                    voltage_matrix, current_times[i], selected_channels, tmpl, params.search_window);
            if (shift != 0) {
                int64_t const new_t = current_times[i] + shift;
                if (new_t >= params.w_pre && new_t + params.w_post < total_samples) {
                    current_times[i] = new_t;
                    any_shift = true;
                }
            }
        }

        // Convergence check: if no events shifted, stop early
        if (!any_shift) {
            break;
        }
    }

    return current_times;
}

std::vector<double> MultichannelTemplateRealigner::computeSubSampleShifts(
        arma::fmat const & voltage_matrix,
        std::span<int64_t const> event_sample_indices,
        std::span<int const> selected_channels,
        TemplateRealignerParams const & params) {

    if (event_sample_indices.empty() || selected_channels.empty()) {
        return {};
    }

    // 1. Compute converged discrete template
    auto const tmpl = computeTemplate(
            voltage_matrix, event_sample_indices, selected_channels, params.w_pre, params.w_post);

    // 2. Find continuous sub-sample shift for each event
    std::vector<double> shifts(event_sample_indices.size(), 0.0);
    for (size_t i = 0; i < event_sample_indices.size(); ++i) {
        shifts[i] = findOptimalSubSampleShift(
                voltage_matrix,
                event_sample_indices[i],
                selected_channels,
                tmpl,
                params.search_window,
                params.sinc_sub_divisions,
                params.sinc_kernel_half_width);
    }

    return shifts;
}

}// namespace Neuralyzer::Transforms::V2
