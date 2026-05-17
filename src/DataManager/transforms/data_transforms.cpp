/**
 * @file data_transforms.cpp
 * @brief Transform operation interfaces and shared progress-callback utilities.
 */

#include "data_transforms.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <memory>

namespace {

/// Minimum number of raw progress invocations coalesced into one forwarded update before emitting a
/// debug log (developer visibility for overly chatty transforms).
constexpr std::size_t kProgressThrottleLogMinRawCalls = 50U;

} // namespace

ProgressCallback throttleProgressCallbackToWholePercents(ProgressCallback inner) {
    if (!inner) {
        return {};
    }

    auto last_emitted = std::make_shared<int>(-1);
    auto raw_calls_since_emit = std::make_shared<std::size_t>(0U);

    return [inner = std::move(inner), last_emitted, raw_calls_since_emit](int raw_progress) mutable {
        int const p = std::clamp(raw_progress, 0, 100);
        ++(*raw_calls_since_emit);

        if (*last_emitted == p) {
            return;
        }

        std::size_t const raw_batch = *raw_calls_since_emit;
        *raw_calls_since_emit = 0U;
        *last_emitted = p;

        if (raw_batch >= kProgressThrottleLogMinRawCalls) {
            spdlog::debug(
                    "throttleProgressCallbackToWholePercents: coalesced {} raw progress callbacks "
                    "before forwarding {}%",
                    raw_batch,
                    p);
        }

        inner(p);
    };
}
