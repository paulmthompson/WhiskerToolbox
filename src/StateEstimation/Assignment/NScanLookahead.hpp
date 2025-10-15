#ifndef N_SCAN_LOOKAHEAD_HPP
#define N_SCAN_LOOKAHEAD_HPP

#include "Common.hpp"
#include "Filter/IFilter.hpp"
#include "Tracking/Tracklet.hpp"

#include <vector>

namespace StateEstimation {

/**
     * @brief Scoring function for combining costs across multiple frames.
     * Allows easy switching between simple sum, log-likelihood, or discounted costs.
     */
using HypothesisScoringFunction = std::function<double(std::vector<double> const &)>;

/**
     * @brief Represents a single hypothesis path during N-scan lookahead.
     * Tracks the sequence of assignments, the filter state, and accumulated cost.
     */
struct Hypothesis {
    std::vector<NodeInfo> path;     // Sequence of (frame, entity_id) assignments
    std::unique_ptr<IFilter> filter;// Cloned filter tracking this hypothesis
    FilterState current_state;      // Current filter state
    std::vector<double> frame_costs;// Cost at each frame for scoring
    double total_cost = 0.0;        // Accumulated cost (computed by scoring function)
    bool terminated = false;        // True if hypothesis exceeded threshold

    Hypothesis() = default;
    Hypothesis(Hypothesis &&) = default;
    Hypothesis & operator=(Hypothesis &&) = default;
    Hypothesis(Hypothesis const &) = delete;
    Hypothesis & operator=(Hypothesis const &) = delete;
};

/**
     * @brief Manages multiple hypothesis branches for a single chain during N-scan.
     */
struct ChainHypotheses {
    std::vector<Hypothesis> branches;// Active hypothesis branches
    TimeFrameIndex start_frame;      // Frame where N-scan started
    int scan_depth = 0;              // Current depth into N-scan
};

/**
     * @brief Select best hypothesis after N-scan or early termination.
     * Returns nullopt if multiple paths remain below threshold after N frames (ambiguity persists).
     * 
     * @param hypotheses All hypotheses for a chain
     * @param reached_n_depth True if we've scanned N frames
     * @return Best hypothesis, or nullopt if ambiguity persists
     */
std::optional<Hypothesis> select_best_hypothesis(
        std::vector<Hypothesis> const & hypotheses,
        bool reached_n_depth) {

    // Count non-terminated hypotheses
    std::vector<Hypothesis const *> viable;
    for (auto const & hyp: hypotheses) {
        if (!hyp.terminated) {
            viable.push_back(&hyp);
        }
    }

    // If only one viable path, commit to it
    if (viable.size() == 1) {
        // Need to make a copy since we can't move from const
        Hypothesis result;
        result.path = viable[0]->path;
        result.current_state = viable[0]->current_state;
        result.frame_costs = viable[0]->frame_costs;
        result.total_cost = viable[0]->total_cost;
        result.terminated = viable[0]->terminated;
        // Don't copy filter (we only need final state)
        return result;
    }

    // If no viable paths, return empty (chain terminates)
    if (viable.empty()) {
        return std::nullopt;
    }

    // If we've reached N depth, commit to best hypothesis even if ambiguity persists
    // This implements the "accept sub-optimal decisions" policy
    if (reached_n_depth) {
        Hypothesis const * best = viable[0];
        for (auto const * hyp: viable) {
            if (hyp->total_cost < best->total_cost) {
                best = hyp;
            }
        }

        Hypothesis result;
        result.path = best->path;
        result.current_state = best->current_state;
        result.frame_costs = best->frame_costs;
        result.total_cost = best->total_cost;
        result.terminated = best->terminated;
        return result;
    }

    // Otherwise, continue scanning (early termination case where we haven't reached N yet)
    // This shouldn't normally happen since we break early when viable_count <= 1
    return std::nullopt;
}

/**
     * @brief Default scoring function: simple sum of Mahalanobis distances.
     * Can be replaced with log-likelihood or discounted sum.
     */
double score_hypothesis_simple_sum(std::vector<double> const & frame_costs) {
    double sum = 0.0;
    for (double cost: frame_costs) {
        sum += cost;
    }
    return sum;
}


}// namespace StateEstimation

#endif// N_SCAN_LOOKAHEAD_HPP