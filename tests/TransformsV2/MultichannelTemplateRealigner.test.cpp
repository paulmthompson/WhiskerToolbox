/**
 * @file MultichannelTemplateRealigner.test.cpp
 * @brief Unit tests for MultichannelTemplateRealigner.
 */

#include "TransformsV2/utils/MultichannelTemplateRealigner.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <armadillo>
#include <vector>

using namespace Neuralyzer::Transforms::V2;
using Catch::Matchers::WithinAbs;

namespace {

[[nodiscard]] arma::fmat createSyntheticMultichannelRecording(
        size_t num_samples,
        size_t num_channels,
        std::vector<int64_t> const & spike_times) {

    arma::fmat mat(num_samples, num_channels, arma::fill::zeros);

    for (int64_t const t: spike_times) {
        if (t + 10 < static_cast<int64_t>(num_samples)) {
            // Channel 0: Strong biphasic spike (trough @ t, peak @ t+5)
            mat(static_cast<size_t>(t - 2), 0) = -20.0f;
            mat(static_cast<size_t>(t), 0) = -100.0f;// trough
            mat(static_cast<size_t>(t + 2), 0) = -30.0f;
            mat(static_cast<size_t>(t + 5), 0) = +80.0f; // peak
            mat(static_cast<size_t>(t + 8), 0) = +10.0f;

            // Channel 1: Attenuated spike
            mat(static_cast<size_t>(t), 1) = -40.0f;
            mat(static_cast<size_t>(t + 5), 1) = +30.0f;
        }
    }

    return mat;
}

}// namespace

TEST_CASE("MultichannelTemplateRealigner computeTemplate", "[TransformsV2][MultichannelTemplateRealigner]") {
    size_t const num_samples = 500;
    size_t const num_channels = 2;
    std::vector<int64_t> const spike_times = {100, 200, 300};

    auto const mat = createSyntheticMultichannelRecording(num_samples, num_channels, spike_times);
    std::vector<int> const channels = {0, 1};

    auto const tmpl = MultichannelTemplateRealigner::computeTemplate(
            mat, spike_times, channels, 5, 10);

    REQUIRE(tmpl.num_channels == 2);
    REQUIRE(tmpl.num_points == 16); // 5 + 10 + 1 = 16

    // Trough is at relative offset dt = 0 -> index w_pre = 5
    float const * ch0 = tmpl.channelData(0);
    REQUIRE_THAT(ch0[5], WithinAbs(-100.0f, 1e-3f));
    // Peak is at relative offset dt = +5 -> index 5 + 5 = 10
    REQUIRE_THAT(ch0[10], WithinAbs(+80.0f, 1e-3f));
}

TEST_CASE("MultichannelTemplateRealigner removes temporal jitter and converges", "[TransformsV2][MultichannelTemplateRealigner]") {
    size_t const num_samples = 1000;
    size_t const num_channels = 2;
    // Ground truth spikes at 200, 400, 600
    std::vector<int64_t> const ground_truth = {200, 400, 600};
    auto const mat = createSyntheticMultichannelRecording(num_samples, num_channels, ground_truth);

    // Add artificial jitter: spike 0 is shifted by +2, spike 1 by -3, spike 2 is exact
    std::vector<int64_t> const jittered_events = {202, 397, 600};
    std::vector<int> const channels = {0, 1};

    TemplateRealignerParams params;
    params.w_pre = 8;
    params.w_post = 15;
    params.search_window = 6;
    params.max_iterations = 5;

    auto const realigned = MultichannelTemplateRealigner::realignEventsDiscrete(
            mat, jittered_events, channels, params);

    REQUIRE(realigned.size() == 3);
    REQUIRE(realigned[0] == 200);
    REQUIRE(realigned[1] == 400);
    REQUIRE(realigned[2] == 600);
}
