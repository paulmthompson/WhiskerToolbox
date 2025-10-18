#ifndef MIN_COST_FLOW_TRACKER_HPP
#define MIN_COST_FLOW_TRACKER_HPP

#include "Assignment/IAssigner.hpp"
#include "Assignment/NScanLookahead.hpp"
#include "Assignment/hungarian.hpp"
#include "Cost/CostFunctions.hpp"
#include "DataSource.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Features/IFeatureExtractor.hpp"
#include "Filter/IFilter.hpp"
#include "Filter/Kalman/KalmanMatrixBuilder.hpp"
#include "MinCostFlowSolver.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "Tracking/AnchorUtils.hpp"
#include "Tracking/Tracklet.hpp"

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"
#include <Eigen/Dense>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

/*
 data objects have features extracted in a time series. 
 These time series of features have a filter applied to find "tracklets" or "meta nodes" that represent small 
 time series of features that represent the same object across multiple frames. Once the tracklets are determined, 
 sparse labels are used to try to assign IDs to each entity in the tracklets. To do this, we perform the 
 following: we order or labels into pairs of time representing the nearest neighbor 
 times (e.g. 1-1000, 1000-4000, 4000-10000 etc). we identify the frames with labels for a group. We construct a 
 subset of meta nodes that repesent 1) the first frame in the tracklet assigned to that label along with the rest 
 of the tracklet (source tracklet). if the label does not exist on the left side of this tracklet, the tracklet is 
 modified into a sliced meta nodet; 2) the tracklet assigned to the last frame from its start to the end frame sliced 
 to remove frames after the anchor; 3) all meta nodes into between these. WE then apply a minimum cost flow solver to 
 find which tracklets can "link" the left and right sliced meta nodes. If a minimum cost flow solution is found, we add all 
 entity IDs on that path to the group that label corresponds to. If the min cost flow solver fails (i.e. because there is a 
 large gap somewhere between tracklets), we will just assign all the entities in the sliced tracklets attached to the 
 anchors to the label group. We then repeat this procedure for all label pairs.

*/

namespace StateEstimation {

/**
 * @brief Contract policy for how MinCostFlowTracker handles invariant violations.
 */
enum class TrackerContractPolicy {
    Throw,         // Throw std::logic_error
    LogAndContinue,// Log error and continue with best-effort result
    Abort          // Log critical and abort process
};

struct TrackerDiagnostics {
    std::size_t noOptimalPathCount = 0;// Number of times solver found no optimal path
};

/**
 * @brief A tracker that uses a global min-cost flow optimization to solve data association.
 *
 * This tracker formulates the tracking problem as a graph problem, finding the globally
 * optimal set of tracks over an entire interval between anchors. It is more robust to
 * ambiguities and identity swaps than iterative, frame-by-frame methods.
 *
 * @tparam DataType raw observation type (e.g., Line2D)
 */
template<typename DataType>
class MinCostFlowTracker {
public:

    /**
     * @brief Construct a new MinCostFlowTracker
     *
     * @param filter_prototype Prototype filter (cloned for prediction and final smoothing). 
     *        If nullptr, prediction is skipped in cost calculation (cost function must handle this)
     *        and no smoothing is performed. The filter's uncertainty automatically scales with
     *        gap size through process noise accumulation.
     * @param feature_extractor Feature extractor for DataType
     * @param cost_function Function to compute cost between predicted state and observation
     * @param cost_scale_factor Multiplier to convert floating-point costs to integers for the solver.
     * @param cheap_assignment_threshold Threshold for greedy chaining
     * @param policy Contract violation policy
     * @param n_scan_depth Number of frames to look ahead when assignments are ambiguous (default 3)
     * @param enable_n_scan Enable N-scan lookahead for ambiguous assignments (default true)
     * @param max_gap_frames Maximum frames a chain can skip before terminating (default 3, set to -1 for unlimited)
     */
    MinCostFlowTracker(std::unique_ptr<IFilter> filter_prototype,
                       std::unique_ptr<IFeatureExtractor<DataType>> feature_extractor,
                       CostFunction cost_function,
                       double cost_scale_factor = 100.0,
                       double cheap_assignment_threshold = 5.0,
                       TrackerContractPolicy policy = TrackerContractPolicy::Throw,
                       int n_scan_depth = 3,
                       bool enable_n_scan = true,
                       int max_gap_frames = 3)
        : _filter_prototype(std::move(filter_prototype)),
          _feature_extractor(std::move(feature_extractor)),
          _chain_cost_function(cost_function),
          _transition_cost_function(cost_function),
          _lookahead_cost_function(cost_function),
          _cost_scale_factor(cost_scale_factor),
          _cheap_assignment_threshold(cheap_assignment_threshold),
          _policy(policy),
          _n_scan_depth(n_scan_depth),
          _enable_n_scan(enable_n_scan),
          _max_gap_frames(max_gap_frames) {}

    /**
     * @brief Construct with separate cost functions for greedy chaining and meta-node transitions.
     *
     * @param chain_cost_function Cost for frame-to-frame greedy chaining (typically 1-step)
     * @param transition_cost_function Cost for meta-node transitions across k-step gaps
     * @param n_scan_depth Number of frames to look ahead when assignments are ambiguous (default 3)
     * @param enable_n_scan Enable N-scan lookahead for ambiguous assignments (default true)
     */
    MinCostFlowTracker(std::unique_ptr<IFilter> filter_prototype,
                       std::unique_ptr<IFeatureExtractor<DataType>> feature_extractor,
                       CostFunction chain_cost_function,
                       CostFunction transition_cost_function,
                       double cost_scale_factor,
                       double cheap_assignment_threshold,
                       TrackerContractPolicy policy = TrackerContractPolicy::Throw,
                       int n_scan_depth = 3,
                       bool enable_n_scan = true,
                       int max_gap_frames = 3)
        : _filter_prototype(std::move(filter_prototype)),
          _feature_extractor(std::move(feature_extractor)),
          _chain_cost_function(chain_cost_function),
          _transition_cost_function(std::move(transition_cost_function)),
          _lookahead_cost_function(chain_cost_function),
          _cost_scale_factor(cost_scale_factor),
          _cheap_assignment_threshold(cheap_assignment_threshold),
          _policy(policy),
          _n_scan_depth(n_scan_depth),
          _enable_n_scan(enable_n_scan),
          _max_gap_frames(max_gap_frames) {}

    /**
     * @brief Convenience constructor using default Mahalanobis distance cost function.
     *
     * @param filter_prototype Prototype filter (cloned for prediction and final smoothing).
     *        If nullptr, Mahalanobis distance cannot be computed properly (requires filter state covariance).
     *        The filter's uncertainty automatically scales with gap size.
     * @param feature_extractor Feature extractor for DataType
     * @param measurement_matrix H matrix for Mahalanobis distance calculation
     * @param measurement_noise_covariance R matrix for Mahalanobis distance calculation
     * @param cost_scale_factor Multiplier to convert floating-point costs to integers for the solver.
     * @param cheap_assignment_threshold Threshold for greedy chaining
     * @param policy Contract violation policy
     * @param n_scan_depth Number of frames to look ahead when assignments are ambiguous (default 3)
     * @param enable_n_scan Enable N-scan lookahead for ambiguous assignments (default true)
     */
    MinCostFlowTracker(std::unique_ptr<IFilter> filter_prototype,
                       std::unique_ptr<IFeatureExtractor<DataType>> feature_extractor,
                       Eigen::MatrixXd const & measurement_matrix,
                       Eigen::MatrixXd const & measurement_noise_covariance,
                       double cost_scale_factor = 100.0,
                       double cheap_assignment_threshold = 5.0,
                       TrackerContractPolicy policy = TrackerContractPolicy::Throw,
                       int n_scan_depth = 3,
                       bool enable_n_scan = true,
                       int max_gap_frames = 3)
        : MinCostFlowTracker(std::move(filter_prototype),
                             std::move(feature_extractor),
                             createMahalanobisCostFunction(measurement_matrix, measurement_noise_covariance),
                             cost_scale_factor,
                             cheap_assignment_threshold,
                             policy,
                             n_scan_depth,
                             enable_n_scan,
                             max_gap_frames) {}

    /**
     * @brief Set a dedicated cost function for N-scan lookahead scoring.
     *
     * This function is used exclusively inside the lookahead expansion and can
     * differ from the greedy chaining or meta-node transition costs. It is
     * useful to introduce dynamics-aware penalties (velocity/acceleration) only
     * for ambiguity resolution while keeping cheaper costs elsewhere.
     *
     * @pre cost_fn should be a valid callable; behavior is undefined if empty
     * @post Subsequent N-scan calls will use the provided function
     */
    void setLookaheadCostFunction(CostFunction cost_fn) {
        _lookahead_cost_function = std::move(cost_fn);
    }

    /**
     * @brief Override the transition cost used between meta-nodes in the MCF graph.
     */
    void setTransitionCostFunction(CostFunction cost_fn) {
        _transition_cost_function = std::move(cost_fn);
    }

    /**
     * @brief Set the acceptance threshold for N-scan lookahead costs.
     *
     * Use a larger threshold for dynamics-aware costs whose scale exceeds the
     * cheap chaining threshold. Set to infinity to disable pruning by threshold.
     */
    void setLookaheadThreshold(double threshold) { _lookahead_threshold = threshold; }

    /**
     * @brief Set ambiguity threshold and optional margin to decide when to run N-scan.
     * If the best cost < ambiguity_threshold and (second_best - best) >= ambiguity_margin,
     * the chain is considered certain and N-scan is skipped.
     */
    void setAmbiguityThreshold(double threshold) { _ambiguity_threshold = threshold; }
    void setAmbiguityMargin(double margin) { _ambiguity_margin = margin; }

    /**
     * @brief Process a range of frames using min-cost flow optimization.
     *
     * @param data_source Zero-copy data source
     * @param group_manager Group manager to record final assignments
     * @param ground_truth Ground truth at specific frames (anchors)
     * @param start_frame Inclusive start frame
     * @param end_frame Inclusive end frame
     * @param progress Optional progress callback
     * @return Smoothed states per group across processed frames
     */
    template<typename Source>
        requires DataSource<Source, DataType>
    [[nodiscard]] SmoothedResults process(Source && data_source,
                                          EntityGroupManager & group_manager,
                                          GroundTruthMap const & ground_truth,
                                          TimeFrameIndex start_frame,
                                          TimeFrameIndex end_frame,
                                          ProgressCallback progress,
                                          std::map<GroupId, GroupId> const * output_group_ids = nullptr,
                                          std::unordered_set<EntityId> const * excluded_entities = nullptr,
                                          std::unordered_set<EntityId> const * include_entities = nullptr) {
        if (_logger) {
            _logger->debug("MCF process: start={} end={}", start_frame.getValue(), end_frame.getValue());
        }

        auto frame_lookup = buildFrameLookup<Source, DataType>(data_source, start_frame, end_frame);

        // Print ground truth map contents
        if (_logger) {
            _logger->debug("Ground truth map contents:");
            for (auto const & [frame, group_entities]: ground_truth) {
                _logger->debug("  Frame {}:", frame.getValue());
                for (auto const & [group_id, entity_id]: group_entities) {
                    _logger->debug("    Group {}: Entity {}", static_cast<unsigned long long>(group_id), static_cast<unsigned long long>(entity_id));
                }
            }
        }

        // 1. --- Build and Solve the Graph ---
        auto solved_paths = solve_flow_problem_new(frame_lookup,
                                                   ground_truth,
                                                   start_frame,
                                                   end_frame,
                                                   progress);

        if (solved_paths.empty()) {
            if (_logger) _logger->error("Min-cost flow solver failed or found no paths.");
            return {};
        }

        // 2. --- Update Group Manager with Solved Tracks ---
        for (auto const & [group_id, path]: solved_paths) {
            GroupId write_group = group_id;
            if (output_group_ids) {
                auto it = output_group_ids->find(group_id);
                if (it != output_group_ids->end()) write_group = it->second;
            }
            for (auto const & node: path) {
                // Never overwrite anchors or any labeled entity: only add unlabeled entities
                // Additionally, skip any frame that already has ground truth for this group to avoid double assignment
                auto gt_frame_it = ground_truth.find(node.frame);
                if (gt_frame_it != ground_truth.end()) {
                    auto const & gt_map = gt_frame_it->second;
                    if (gt_map.find(group_id) != gt_map.end()) continue;
                }
                auto groups = group_manager.getGroupsContainingEntity(node.entity_id);
                if (!groups.empty()) continue;
                group_manager.addEntityToGroup(write_group, node.entity_id);
            }
        }

        // 3. --- Final Forward/Backward Smoothing Pass ---
        // Now that we have the globally optimal assignments, run a final KF pass to get the smoothed states.
        return generate_smoothed_results(solved_paths, frame_lookup, start_frame, end_frame);
    }

    void enableDebugLogging(std::string const & file_path) {
        _logger = std::make_shared<spdlog::logger>("MinCostFlowTracker", std::make_shared<spdlog::sinks::basic_file_sink_mt>(file_path, true));
        _logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        _logger->set_level(spdlog::level::debug);
        _logger->flush_on(spdlog::level::debug);
    }

    [[nodiscard]] TrackerDiagnostics getDiagnostics() const { return _diagnostics; }

private:
    // Structure to track active chains being built
    struct ActiveChain {
        size_t meta_node_idx;// Index in meta_nodes vector
        TimeFrameIndex curr_frame;
        EntityId curr_entity;
        DataType const * curr_data;
        std::unique_ptr<IFilter> filter;// Cloned filter for this chain
        FilterState predicted;          // Cached prediction for next frame
        std::vector<NodeInfo> members;  // Collected nodes for this chain
        FilterState start_state;        // Initial state at chain start (for meta-node)

        // Constructor to properly initialize TimeFrameIndex
        ActiveChain()
            : meta_node_idx(0),
              curr_frame(TimeFrameIndex(0)),
              curr_entity(0),
              curr_data(nullptr) {}
    };


    /**
     * @brief Solve a single ground-truth segment over trimmed meta-nodes.
     *
     * Expects the anchors to be present in the provided meta-nodes (ideally after slicing).
     * Builds a min-cost single-unit path through the meta-graph and expands it to a full path.
     *
     * @param meta_nodes_trimmed Meta-nodes restricted to the segment range
     * @param frame_lookup Frame data lookup for cost evaluation
     * @param group_id Group identifier (for logging/diagnostics)
     * @param segment Ground truth segment describing anchors
     * @return Expanded path (sequence of NodeInfo) for the segment; empty on failure
     */
    Path solve_single_segment_flow_over_meta(
            std::vector<MetaNode> const & meta_nodes_trimmed,
            std::map<TimeFrameIndex, FrameBucket<DataType>> const & frame_lookup,
            GroupId group_id,
            GroundTruthSegment const & segment) {

        if (_logger) {
            _logger->debug("Solving single segment flow over meta: group={} start=({}, {}) end=({}, {})",
                           static_cast<unsigned long long>(group_id),
                           segment.start_frame.getValue(), segment.start_entity,
                           segment.end_frame.getValue(), segment.end_entity);
        }

        // Fast path: check if a single meta-node spans the segment exactly
        for (auto const & mn: meta_nodes_trimmed) {
            if (!mn.members.empty() &&
                mn.members.front().frame == segment.start_frame &&
                mn.members.front().entity_id == segment.start_entity &&
                mn.members.back().frame == segment.end_frame &&
                mn.members.back().entity_id == segment.end_entity) {
                return mn.members;
            }
        }

        // Find anchor positions within trimmed set
        auto const pos_opt = findAnchorPositions(meta_nodes_trimmed, segment);
        if (!pos_opt.has_value()) {
            if (_logger) {
                _logger->error("Segment anchors not found in trimmed meta-nodes: group={} start=({}, {}) end=({}, {})",
                               static_cast<unsigned long long>(group_id),
                               segment.start_frame.getValue(), segment.start_entity,
                               segment.end_frame.getValue(), segment.end_entity);
            }
            return {};
        }
        int const start_meta_index = pos_opt->start_meta_index;
        size_t const start_member_index = pos_opt->start_member_index;
        int const end_meta_index = pos_opt->end_meta_index;
        size_t const end_member_index = pos_opt->end_member_index;

        int const num_meta = static_cast<int>(meta_nodes_trimmed.size());
        int const source_node = num_meta;
        int const sink_node = num_meta + 1;

        std::vector<ArcSpec> arcs;
        arcs.reserve(static_cast<size_t>(num_meta * num_meta / 4 + 4));
        arcs.push_back({source_node, start_meta_index, 1, 0});
        arcs.push_back({end_meta_index, sink_node, 1, 0});

        // Build transition arcs (forward in time only)
        int num_transition_arcs = 0;
        constexpr int64_t kMaxHorizon = 50;
        for (int i = 0; i < num_meta; ++i) {
            MetaNode const & from = meta_nodes_trimmed[static_cast<size_t>(i)];
            for (int j = 0; j < num_meta; ++j) {
                MetaNode const & to = meta_nodes_trimmed[static_cast<size_t>(j)];
                if (to.start_frame <= from.end_frame) continue;
                int const steps = (to.start_frame - from.end_frame).getValue();
                if (steps <= 0 || steps > kMaxHorizon) continue;

                FilterState predicted_state;
                if (_filter_prototype) {
                    auto temp_filter = _filter_prototype->clone();
                    // Coerce end_state if needed
                    FilterState init_state = from.end_state;
                    int const target_dim = static_cast<int>(temp_filter->getState().state_mean.size());
                    if (static_cast<int>(init_state.state_mean.size()) != target_dim ||
                        init_state.state_covariance.rows() != target_dim ||
                        init_state.state_covariance.cols() != target_dim) {
                        FilterState coerced;
                        coerced.state_mean = Eigen::VectorXd::Zero(target_dim);
                        int const copy_dim = std::min<int>(target_dim, static_cast<int>(init_state.state_mean.size()));
                        if (copy_dim > 0) coerced.state_mean.head(copy_dim) = init_state.state_mean.head(copy_dim);
                        coerced.state_covariance = Eigen::MatrixXd::Zero(target_dim, target_dim);
                        int const cr = std::min<int>(target_dim, init_state.state_covariance.rows());
                        int const cc = std::min<int>(target_dim, init_state.state_covariance.cols());
                        if (cr > 0 && cc > 0) {
                            int const b = std::min(cr, cc);
                            coerced.state_covariance.topLeftCorner(b, b) = init_state.state_covariance.topLeftCorner(b, b);
                        }
                        constexpr double kPadVar = 1e6;
                        for (int d = 0; d < target_dim; ++d) {
                            if (coerced.state_covariance(d, d) <= 0.0) coerced.state_covariance(d, d) = kPadVar;
                        }
                        init_state = std::move(coerced);
                    }
                    temp_filter->initialize(init_state);
                    for (int s = 0; s < steps; ++s) {
                        predicted_state = temp_filter->predict();
                    }
                }

                DataType const * to_start_data = findEntity(frame_lookup.at(to.start_frame), to.start_entity);
                if (!to_start_data) continue;
                Eigen::VectorXd obs = _feature_extractor->getFilterFeatures(*to_start_data);
                double const dist = _transition_cost_function(predicted_state, obs, steps);
                int64_t const arc_cost = static_cast<int64_t>(dist * _cost_scale_factor);
                arcs.push_back({i, j, 1, arc_cost});
                num_transition_arcs++;
            }
        }

        auto const seq_opt = solveMinCostSingleUnitPath(num_meta + 2, source_node, sink_node, arcs);
        if (!seq_opt.has_value()) {
            _diagnostics.noOptimalPathCount += 1;
            if (_logger) {
                _logger->error("Min-cost flow failed for segment: group={} metaNodes={} arcs={} — falling back to anchors only",
                               static_cast<unsigned long long>(group_id), num_meta, arcs.size());
            }
            return buildFallbackPathFromTrimmed(meta_nodes_trimmed, start_meta_index, end_meta_index);
        }

        Path expanded_path;
        auto const & sequence = *seq_opt;
        for (size_t idx = 1; idx < sequence.size(); ++idx) {
            int const node_index = sequence[idx];
            if (node_index >= 0 && node_index < num_meta) {
                auto const & mem = meta_nodes_trimmed[static_cast<size_t>(node_index)].members;
                for (auto const & n: mem) expanded_path.push_back(n);
            }
        }
        return expanded_path;
    }

    /**
     * @brief Solve paths for all groups by iterating ground-truth segments and concatenating.
     *
     * For each consecutive labeled segment per group, slice meta-nodes to the segment,
     * run a per-segment min-cost path, and append the nodes to the group's output path.
     *
     * Deduplicates a single overlapping anchor node at segment boundaries.
     */
    std::map<GroupId, Path> solve_flow_over_segments(
            std::vector<MetaNode> const & meta_nodes,
            std::map<TimeFrameIndex, FrameBucket<DataType>> const & frame_lookup,
            GroundTruthMap const & ground_truth,
            TimeFrameIndex start_frame,
            TimeFrameIndex end_frame) {

        std::map<GroupId, Path> solved_paths;
        auto const segments = extractGroundTruthSegments(ground_truth);

        // Process segments in chronological order per group
        std::map<GroupId, std::vector<GroundTruthSegment>> by_group;
        for (auto const & seg: segments) {
            // Optionally filter to the requested range
            if (seg.end_frame < start_frame || seg.start_frame > end_frame) continue;
            by_group[seg.group_id].push_back(seg);
        }
        for (auto & [gid, segs]: by_group) {
            std::sort(segs.begin(), segs.end(), [](auto const & a, auto const & b) {
                return a.start_frame < b.start_frame;
            });
            Path out_path;
            for (auto const & seg: segs) {
                auto trimmed = sliceMetaNodesToSegment(meta_nodes, seg);
                if (trimmed.empty()) {
                    if (_logger) {
                        _logger->warn("No trimmed meta-nodes for segment: group={} start=({}, {}) end=({}, {})",
                                      static_cast<unsigned long long>(gid),
                                      seg.start_frame.getValue(), seg.start_entity,
                                      seg.end_frame.getValue(), seg.end_entity);
                    }
                    continue;
                }

                if (_logger) {
                    _logger->debug("Solving segment: group={} start=({}, {}) end=({}, {})",
                                   static_cast<unsigned long long>(gid),
                                   seg.start_frame.getValue(), seg.start_entity,
                                   seg.end_frame.getValue(), seg.end_entity);
                }

                Path segment_path = solve_single_segment_flow_over_meta(trimmed, frame_lookup, gid, seg);
                if (segment_path.empty()) continue;

                // Deduplicate overlapping anchor between consecutive segments
                if (!out_path.empty() && !segment_path.empty()) {
                    auto const & last = out_path.back();
                    auto const & first = segment_path.front();
                    if (last.frame == first.frame && last.entity_id == first.entity_id) {
                        segment_path.erase(segment_path.begin());
                    }
                }
                // Append
                out_path.insert(out_path.end(), segment_path.begin(), segment_path.end());
            }
            if (!out_path.empty()) {
                solved_paths.emplace(gid, std::move(out_path));
            }
        }

        return solved_paths;
    }

    // --- Main Graph Building and Solving Logic ---
    std::map<GroupId, Path> solve_flow_problem_new(
            std::map<TimeFrameIndex, FrameBucket<DataType>> const & frame_lookup,
            GroundTruthMap const & ground_truth,
            TimeFrameIndex start_frame,
            TimeFrameIndex end_frame,
            ProgressCallback progress) {


        auto start_anchors_it = ground_truth.find(start_frame);
        auto end_anchors_it = ground_truth.find(end_frame);

        if (start_anchors_it == ground_truth.end() || end_anchors_it == ground_truth.end()) {
            if (_logger) _logger->error("Min-cost flow requires anchors at both start and end frames.");
            return {};
        }

        auto start_anchors = start_anchors_it->second;
        auto end_anchors = end_anchors_it->second;

        // 1) Build greedy meta-nodes (cheap consecutive links) independent of groups
        auto meta_nodes = build_meta_nodes(frame_lookup, start_frame, end_frame, progress);

        // 2) Solve paths per group by iterating ground-truth segments and concatenating
        auto all_solved_paths = solve_flow_over_segments(meta_nodes,
                                                         frame_lookup,
                                                         ground_truth,
                                                         start_frame,
                                                         end_frame);

        return all_solved_paths;
    }

    /**
     * @brief Build meta-nodes using Hungarian algorithm for optimal chain extension.
     * 
     * Unlike greedy assignment, this uses Hungarian algorithm at each frame to ensure
     * global optimal assignment of chains to candidates, preventing "stealing" where
     * one chain takes another's best match.
     * 
     * @pre frame_lookup contains observations in [start_frame, end_frame]
     * @post Each observation belongs to at most one meta-node
     */
    std::vector<MetaNode> build_meta_nodes(
            std::map<TimeFrameIndex, FrameBucket<DataType>> const & frame_lookup,
            TimeFrameIndex start_frame,
            TimeFrameIndex end_frame,
            ProgressCallback progress) {


        progress(0);

        std::vector<MetaNode> meta_nodes;
        std::set<std::pair<long long, EntityId>> used;// key: (frame, entity)
        std::vector<ActiveChain> active_chains;


        // Process frame by frame, using Hungarian algorithm to extend chains optimally
        for (TimeFrameIndex f = start_frame; f <= end_frame; ++f) {
            if (!frame_lookup.count(f)) continue;

            if (_logger) {
                _logger->debug("Processing frame {}: {} active chains, {} observations",
                               f.getValue(), active_chains.size(), frame_lookup.at(f).size());
            }

            std::unordered_set<EntityId> this_frame_entities;
            for (size_t cand_idx = 0; cand_idx < frame_lookup.at(f).size(); ++cand_idx) {
                auto const & cand = frame_lookup.at(f)[cand_idx];
                this_frame_entities.insert(std::get<1>(cand));
            }

            // Step 1: Try to extend existing active chains to current frame (if any)
            // This must happen BEFORE creating new chains, so that chains can jump gaps
            if (!active_chains.empty() && f > start_frame) {

                // Predict all remaining active chains forward to current frame
                for (size_t chain_idx = 0; chain_idx < active_chains.size(); ++chain_idx) {
                    auto & chain = active_chains[chain_idx];
                    if (chain.filter) {
                        // Predict forward from last known frame to current frame
                        int gap_frames = static_cast<int>(f.getValue() - chain.curr_frame.getValue());
                        if (_logger && gap_frames > 0) {
                            auto initial_state = chain.filter->getState();
                            _logger->debug("Chain {} at frame {} before predictions: state=[{:.2f},{:.2f},{:.2f},{:.2f}], curr_entity={}",
                                           chain_idx, f.getValue(),
                                           initial_state.state_mean(0), initial_state.state_mean(1),
                                           initial_state.state_mean(2), initial_state.state_mean(3),
                                           chain.curr_entity);
                        }
                        for (int step = 0; step < gap_frames; ++step) {
                            chain.predicted = chain.filter->predict();
                            if (_logger && gap_frames > 1) {
                                _logger->debug("  After predict step {}/{}: state=[{:.2f},{:.2f},{:.2f},{:.2f}]",
                                               step + 1, gap_frames,
                                               chain.predicted.state_mean(0), chain.predicted.state_mean(1),
                                               chain.predicted.state_mean(2), chain.predicted.state_mean(3));
                            }
                        }
                        if (_logger && gap_frames > 0) {
                            _logger->debug("  Final predicted state: [{:.2f},{:.2f},{:.2f},{:.2f}]",
                                           chain.predicted.state_mean(0), chain.predicted.state_mean(1),
                                           chain.predicted.state_mean(2), chain.predicted.state_mean(3));
                        }
                    }
                }

                // Collect available candidates at current frame
                std::vector<std::tuple<EntityId, DataType const *, size_t>> candidates;
                for (size_t cand_idx = 0; cand_idx < frame_lookup.at(f).size(); ++cand_idx) {
                    auto const & cand = frame_lookup.at(f)[cand_idx];
                    EntityId cand_id = std::get<1>(cand);
                    auto key = std::make_pair(static_cast<long long>(f.getValue()), cand_id);
                    if (used.count(key)) continue;// How could this be true?

                    DataType const * cand_data = std::get<0>(cand);
                    candidates.emplace_back(cand_id, cand_data, cand_idx);
                }

                if (!candidates.empty() && !active_chains.empty()) {
                    // Build cost matrix for Hungarian algorithm
                    int const cost_scaling_factor = 1000;
                    int const max_cost = static_cast<int>(_cheap_assignment_threshold * cost_scaling_factor);
                    std::vector<std::vector<int>> cost_matrix(active_chains.size(),
                                                              std::vector<int>(candidates.size()));

                    for (size_t chain_idx = 0; chain_idx < active_chains.size(); ++chain_idx) {
                        auto const & chain = active_chains[chain_idx];
                        for (size_t cand_idx = 0; cand_idx < candidates.size(); ++cand_idx) {
                            DataType const * cand_data = std::get<1>(candidates[cand_idx]);
                            Eigen::VectorXd obs = _feature_extractor->getFilterFeatures(*cand_data);

                            double cost_double;
                            if (chain.filter) {
                                int gap_frames = static_cast<int>(f.getValue() - chain.curr_frame.getValue());
                                cost_double = _chain_cost_function(chain.predicted, obs, gap_frames);
                            } else {
                                Eigen::VectorXd curr_obs = _feature_extractor->getFilterFeatures(*chain.curr_data);
                                cost_double = (curr_obs - obs).norm();
                            }

                            int cost = static_cast<int>(cost_double * cost_scaling_factor);
                            if (cost >= std::numeric_limits<int>::max()) {
                                cost = std::numeric_limits<int>::max() - 1;
                            }
                            cost_matrix[chain_idx][cand_idx] = cost;
                        }
                    }

                    // Solve Hungarian assignment
                    std::vector<std::vector<int>> assignment_matrix;
                    Munkres::hungarian_with_assignment(cost_matrix, assignment_matrix);

                    // Check for ambiguity and trigger N-scan if enabled
                    std::unordered_set<size_t> ambiguous_chain_indices;
                    if (_enable_n_scan && _filter_prototype) {
                        Eigen::MatrixXd cost_matrix_eigen(active_chains.size(), candidates.size());
                        for (size_t i = 0; i < active_chains.size(); ++i) {
                            for (size_t j = 0; j < candidates.size(); ++j) {
                                cost_matrix_eigen(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)) =
                                        cost_matrix[i][j] / static_cast<double>(cost_scaling_factor);
                            }
                        }
                        ambiguous_chain_indices = detect_ambiguous_chains(cost_matrix_eigen, _ambiguity_threshold);
                        // Apply certainty margin: drop chains whose best is clearly better than next-best
                        if (_ambiguity_margin > 0.0) {
                            std::unordered_set<size_t> pruned;
                            for (size_t i = 0; i < active_chains.size(); ++i) {
                                if (ambiguous_chain_indices.find(i) == ambiguous_chain_indices.end()) continue;
                                // compute best and second best
                                double best = std::numeric_limits<double>::infinity();
                                double second = std::numeric_limits<double>::infinity();
                                for (size_t j = 0; j < candidates.size(); ++j) {
                                    double c = cost_matrix_eigen(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j));
                                    if (c < best) {
                                        second = best;
                                        best = c;
                                    } else if (c < second) {
                                        second = c;
                                    }
                                }
                                if (best < _ambiguity_threshold && (second - best) >= _ambiguity_margin) {
                                    pruned.insert(i);
                                }
                            }
                            for (auto idx: pruned) ambiguous_chain_indices.erase(idx);
                        }

                        if (_logger && !ambiguous_chain_indices.empty()) {
                            _logger->debug("Frame {}: Detected {} ambiguous chains (threshold={:.3f})",
                                           f.getValue(), ambiguous_chain_indices.size(), _cheap_assignment_threshold);

                            // Check if we can run N-scan
                            int frames_ahead = (end_frame - f).getValue();
                            if (frames_ahead < _n_scan_depth) {
                                _logger->debug("  N-scan SKIPPED: need {} frames ahead, only have {} (end_frame={})",
                                               _n_scan_depth, frames_ahead, end_frame.getValue());
                            }
                        }
                    }

                    // If there are ambiguous chains, run N-scan for ALL of them FIRST, then assign globally
                    std::map<size_t, std::pair<std::vector<NodeInfo>, double>> n_scan_results;// chain_idx -> (path, total_cost)
                    // Variable-depth lookahead: allow shorter depth near tail
                    int frames_ahead_var = (end_frame - f).getValue();
                    int allowable_depth = std::min(_n_scan_depth, frames_ahead_var + 1);
                    if (!ambiguous_chain_indices.empty() && allowable_depth >= 1) {
                        if (_logger) {
                            _logger->debug("  Running N-scan with depth={} (need {} future frames, have {})",
                                           allowable_depth, allowable_depth - 1, (end_frame - f).getValue());
                        }
                        // Step 1: Run N-scan for each ambiguous chain independently
                        std::map<size_t, std::vector<std::pair<std::vector<NodeInfo>, double>>> all_paths;// chain_idx -> [(path, cost), ...]

                        int const saved_depth = _n_scan_depth;
                        _n_scan_depth = allowable_depth;// temporary override for lookahead
                        for (size_t chain_idx: ambiguous_chain_indices) {
                            auto & chain = active_chains[chain_idx];

                            if (_logger) {
                                _logger->debug("  N-scan for chain {} (curr_entity={}, curr_frame={})",
                                               chain_idx, chain.curr_entity, chain.curr_frame.getValue());
                            }

                            // Collect viable candidates with their costs using lookahead cost
                            std::vector<std::tuple<EntityId, DataType const *, double>> viable_candidates;
                            int const gap_frames = static_cast<int>(f.getValue() - chain.curr_frame.getValue());
                            for (size_t cand_idx = 0; cand_idx < candidates.size(); ++cand_idx) {
                                DataType const * cand_data = std::get<1>(candidates[cand_idx]);
                                Eigen::VectorXd obs = _feature_extractor->getFilterFeatures(*cand_data);
                                double cost_double = _lookahead_cost_function(chain.predicted, obs, std::max(1, gap_frames));
                                if (cost_double < _lookahead_threshold || !std::isfinite(_lookahead_threshold)) {
                                    viable_candidates.emplace_back(
                                            std::get<0>(candidates[cand_idx]),
                                            cand_data,
                                            cost_double);
                                }
                            }

                            // Each chain gets its own copy of 'used' to explore independently
                            std::set<std::pair<long long, EntityId>> chain_used = used;
                            auto [n_scan_path, path_cost] = run_n_scan_lookahead(chain, viable_candidates, f, end_frame,
                                                                                 frame_lookup, chain_used);

                            if (!n_scan_path.empty()) {
                                n_scan_results[chain_idx] = {n_scan_path, path_cost};
                            } else if (allowable_depth == 1) {
                                // One-step fallback: pick best single candidate not used
                                double best_c = std::numeric_limits<double>::infinity();
                                EntityId best_eid = 0;
                                for (auto const & tup: viable_candidates) {
                                    EntityId eid = std::get<0>(tup);
                                    auto key = std::make_pair(static_cast<long long>(f.getValue()), eid);
                                    if (used.count(key)) continue;
                                    double c = std::get<2>(tup);
                                    if (c < best_c) {
                                        best_c = c;
                                        best_eid = eid;
                                    }
                                }
                                if (best_eid != 0 && (best_c < _lookahead_threshold || !std::isfinite(_lookahead_threshold))) {
                                    std::vector<NodeInfo> single{{f, best_eid}};
                                    n_scan_results[chain_idx] = {single, best_c};
                                }
                            }
                        }
                        _n_scan_depth = saved_depth;// restore

                        // Step 2: Detect conflicts - check if multiple chains want the same observations
                        if (_logger && !n_scan_results.empty()) {
                            _logger->debug("N-scan completed for {} chains at frame {}", n_scan_results.size(), f.getValue());
                            for (auto const & [chain_idx, path_and_cost]: n_scan_results) {
                                _logger->debug("  Chain {}: cost={:.2f}, path length={}",
                                               chain_idx, path_and_cost.second, path_and_cost.first.size());
                            }
                        }

                        std::map<std::pair<long long, EntityId>, std::vector<size_t>> obs_to_chains;
                        for (auto const & [chain_idx, path_and_cost]: n_scan_results) {
                            // Only the current frame decision participates in conflicts
                            if (!path_and_cost.first.empty()) {
                                auto const & first_node = path_and_cost.first.front();
                                auto key = std::make_pair(static_cast<long long>(first_node.frame.getValue()), first_node.entity_id);
                                obs_to_chains[key].push_back(chain_idx);
                            }
                        }

                        if (_logger && !obs_to_chains.empty()) {
                            _logger->debug("Observation assignment: {} unique observations claimed", obs_to_chains.size());
                            for (auto const & [obs_key, claiming_chains]: obs_to_chains) {
                                if (claiming_chains.size() > 1) {
                                    _logger->debug("  Frame {}, entity {}: {} chains want it",
                                                   obs_key.first, obs_key.second, claiming_chains.size());
                                }
                            }
                        }

                        // Step 3: Resolve conflicts - if multiple chains want same observation, keep lowest cost
                        std::set<size_t> rejected_chains;
                        for (auto const & [obs_key, claiming_chains]: obs_to_chains) {
                            if (claiming_chains.size() > 1) {
                                // Conflict! Keep chain with lowest cost, reject others
                                if (_logger) {
                                    _logger->debug("N-scan conflict at frame {}, entity {}: {} chains competing",
                                                   obs_key.first, obs_key.second, claiming_chains.size());
                                    for (size_t chain_idx: claiming_chains) {
                                        _logger->debug("  Chain {} has cost {:.2f}", chain_idx, n_scan_results[chain_idx].second);
                                    }
                                }

                                size_t best_chain = claiming_chains[0];
                                double best_cost = n_scan_results[best_chain].second;
                                for (size_t chain_idx: claiming_chains) {
                                    if (n_scan_results[chain_idx].second < best_cost) {
                                        best_chain = chain_idx;
                                        best_cost = n_scan_results[chain_idx].second;
                                    }
                                }

                                if (_logger) {
                                    _logger->debug("  Keeping chain {} (cost {:.2f}), rejecting others", best_chain, best_cost);
                                }

                                // Reject all other chains
                                for (size_t chain_idx: claiming_chains) {
                                    if (chain_idx != best_chain) {
                                        rejected_chains.insert(chain_idx);
                                        if (_logger) {
                                            _logger->debug("  Rejected chain {}", chain_idx);
                                        }
                                    }
                                }
                            }
                        }

                        // Step 4: Remove rejected chains from results
                        for (size_t rejected: rejected_chains) {
                            n_scan_results.erase(rejected);
                        }

                        // Step 5: Mark accepted N-scan selections (current frame only) as used
                        for (auto const & [chain_idx, path_and_cost]: n_scan_results) {
                            if (!path_and_cost.first.empty()) {
                                auto const & node = path_and_cost.first.front();
                                used.insert(std::make_pair(static_cast<long long>(node.frame.getValue()), node.entity_id));
                            }
                        }

                        // Step 5b: Attempt fallback N-scan for rejected/failed ambiguous chains
                        // Re-run N-scan for chains that were ambiguous but have no accepted result,
                        // now honoring the updated 'used' set (to avoid prior conflicts).
                        for (size_t chain_idx: ambiguous_chain_indices) {
                            if (n_scan_results.count(chain_idx) > 0) continue;// already accepted
                            auto & chain = active_chains[chain_idx];

                            // Rebuild viable candidates using lookahead cost and current 'used'
                            std::vector<std::tuple<EntityId, DataType const *, double>> viable_candidates_alt;
                            int const gap_frames_alt = static_cast<int>(f.getValue() - chain.curr_frame.getValue());
                            for (size_t cand_idx = 0; cand_idx < candidates.size(); ++cand_idx) {
                                EntityId eid = std::get<0>(candidates[cand_idx]);
                                auto key = std::make_pair(static_cast<long long>(f.getValue()), eid);
                                if (used.count(key)) continue;// avoid already claimed obs
                                DataType const * cand_data = std::get<1>(candidates[cand_idx]);
                                Eigen::VectorXd obs = _feature_extractor->getFilterFeatures(*cand_data);
                                double cost_double = _lookahead_cost_function(chain.predicted, obs, std::max(1, gap_frames_alt));
                                if (cost_double < _lookahead_threshold || !std::isfinite(_lookahead_threshold)) {
                                    viable_candidates_alt.emplace_back(eid, cand_data, cost_double);
                                }
                            }

                            if (!viable_candidates_alt.empty()) {
                                int const saved_depth2 = _n_scan_depth;
                                _n_scan_depth = allowable_depth;// use same allowable depth
                                // Use the updated 'used' set so we avoid previous conflicts
                                auto alt_used = used;// COPY used set
                                auto [alt_path, alt_cost] = run_n_scan_lookahead(chain, viable_candidates_alt, f, end_frame,
                                                                                 frame_lookup, alt_used);
                                _n_scan_depth = saved_depth2;
                                if (!alt_path.empty()) {
                                    // Accept alternate but commit only current frame
                                    std::vector<NodeInfo> single{alt_path.front()};
                                    n_scan_results[chain_idx] = {single, alt_cost};
                                    used.insert(std::make_pair(static_cast<long long>(single.front().frame.getValue()), single.front().entity_id));
                                    if (_logger) {
                                        _logger->debug("  Fallback N-scan accepted for chain {}: cost={:.2f}, eid={}",
                                                       chain_idx, alt_cost, single.front().entity_id);
                                    }
                                } else if (allowable_depth == 1) {
                                    // One-step fallback here too
                                    double best_c = std::numeric_limits<double>::infinity();
                                    EntityId best_eid = 0;
                                    for (auto const & tup: viable_candidates_alt) {
                                        EntityId eid = std::get<0>(tup);
                                        auto key = std::make_pair(static_cast<long long>(f.getValue()), eid);
                                        if (used.count(key)) continue;
                                        double c = std::get<2>(tup);
                                        if (c < best_c) {
                                            best_c = c;
                                            best_eid = eid;
                                        }
                                    }
                                    if (best_eid != 0 && (best_c < _lookahead_threshold || !std::isfinite(_lookahead_threshold))) {
                                        std::vector<NodeInfo> single{{f, best_eid}};
                                        n_scan_results[chain_idx] = {single, best_c};
                                        used.insert(std::make_pair(static_cast<long long>(f.getValue()), best_eid));
                                        if (_logger) {
                                            _logger->debug("  Fallback single-step accepted for chain {}: eid={}, cost={:.2f}",
                                                           chain_idx, best_eid, best_c);
                                        }
                                    }
                                }
                            }
                        }
                    }// if (!ambiguous_chain_indices.empty() && allowable_depth >= 1)

                    // Process assignments
                    std::vector<ActiveChain> remaining_chains;

                    for (size_t chain_idx = 0; chain_idx < assignment_matrix.size(); ++chain_idx) {
                        // Check if this chain has N-scan results
                        if (n_scan_results.count(chain_idx) > 0) {
                            auto & chain = active_chains[chain_idx];
                            auto const & n_scan_path = n_scan_results[chain_idx].first;// current-frame selection

                            // Extend chain with the single current-frame decision
                            if (!n_scan_path.empty()) {
                                auto const & sel = n_scan_path.front();
                                chain.members.push_back(sel);
                                //this_frame_entities.erase(sel.entity_id);
                            }

                            // Update chain to the selected node at current frame
                            auto const & last_node = n_scan_path.front();
                            chain.curr_frame = last_node.frame;
                            chain.curr_entity = last_node.entity_id;
                            this_frame_entities.erase(last_node.entity_id);
                            chain.curr_data = findEntity(frame_lookup.at(last_node.frame), last_node.entity_id);

                            // Re-sync filter
                            if (chain.filter && chain.curr_data) {
                                Eigen::VectorXd obs = _feature_extractor->getFilterFeatures(*chain.curr_data);
                                // At current frame, update with the current predicted
                                chain.filter->update(chain.predicted, Measurement{obs});
                            }

                            remaining_chains.push_back(std::move(chain));
                            continue;
                        }

                        // Check if this chain was ambiguous but N-scan failed
                        if (ambiguous_chain_indices.count(chain_idx) > 0) {
                            auto & chain = active_chains[chain_idx];
                            auto & node = meta_nodes[chain.meta_node_idx];

                            // N-scan failed - terminate chain
                            // Finalize chain into a meta-node upon termination
                            MetaNode term;
                            term.start_frame = chain.members.front().frame;
                            term.start_entity = chain.members.front().entity_id;
                            term.members = chain.members;
                            term.end_frame = chain.members.back().frame;
                            term.end_entity = chain.members.back().entity_id;
                            term.start_state = chain.start_state;
                            if (chain.filter) term.end_state = chain.filter->getState();
                            meta_nodes.push_back(std::move(term));
                            this_frame_entities.erase(chain.members.back().entity_id);
                            continue;
                        }

                        // Normal assignment processing
                        bool found_assignment = false;
                        int assigned_cand_idx = -1;

                        for (size_t cand_idx = 0; cand_idx < assignment_matrix[chain_idx].size(); ++cand_idx) {
                            if (assignment_matrix[chain_idx][cand_idx] == 1) {
                                if (cost_matrix[chain_idx][cand_idx] <= max_cost) {
                                    found_assignment = true;
                                    assigned_cand_idx = static_cast<int>(cand_idx);
                                }
                                break;
                            }
                        }

                        auto & chain = active_chains[chain_idx];

                        if (found_assignment) {
                            // Extend chain
                            EntityId best_entity = std::get<0>(candidates[assigned_cand_idx]);
                            DataType const * best_data = std::get<1>(candidates[assigned_cand_idx]);

                            if (_logger) {
                                double cost_unscaled = static_cast<double>(cost_matrix[chain_idx][assigned_cand_idx]) / cost_scaling_factor;
                                _logger->debug("  Chain {} (entity {}) → entity {} (cost={:.3f}, threshold={:.3f})",
                                               chain_idx, chain.curr_entity, best_entity, cost_unscaled, _cheap_assignment_threshold);
                            }

                            // Guard: if N-scan (or another chain) already committed this obs at current frame,
                            // do not double-claim it. Treat as no assignment and let fallback/termination handle it.
                            auto used_key_current = std::make_pair(static_cast<long long>(f.getValue()), best_entity);
                            if (used.count(used_key_current)) {
                                // I think this might be a false positive
                                //
                                found_assignment = false;
                            }

                            Eigen::VectorXd obs = _feature_extractor->getFilterFeatures(*best_data);
                            if (chain.filter) {
                                chain.filter->update(chain.predicted, {obs});

                                // Check covariance health
                                auto updated_state = chain.filter->getState();
                                double determinant = updated_state.state_covariance.determinant();

                                if (std::abs(determinant) < 1e-10 && _logger) {
                                    Eigen::JacobiSVD<Eigen::MatrixXd> svd(updated_state.state_covariance);
                                    double condition_number = svd.singularValues()(0) /
                                                              (svd.singularValues()(svd.singularValues().size() - 1) + 1e-20);

                                    _logger->warn("State covariance singular: det={:.2e}, cond={:.2e}",
                                                  determinant, condition_number);

                                    if (condition_number > 1e12) {
                                        _logger->warn("  Terminating chain due to ill-conditioned covariance");
                                        found_assignment = false;
                                    }
                                }
                            }

                            if (found_assignment) {
                                NodeInfo next_info{f, best_entity};
                                chain.members.push_back(next_info);
                                used.insert(std::make_pair(static_cast<long long>(f.getValue()), best_entity));

                                chain.curr_frame = f;
                                chain.curr_entity = best_entity;
                                chain.curr_data = best_data;
                                this_frame_entities.erase(best_entity);
                                remaining_chains.push_back(std::move(chain));
                            }
                        }

                        if (!found_assignment) {
                            // Chain terminates -> emit meta-node
                            if (_logger) {
                                _logger->debug("  Chain {} (entity {}) terminated at frame {} - emit meta-node",
                                               chain_idx, chain.curr_entity, chain.curr_frame.getValue());
                            }
                            MetaNode term;
                            term.start_frame = chain.members.front().frame;
                            term.start_entity = chain.members.front().entity_id;
                            term.members = chain.members;
                            term.end_frame = chain.members.back().frame;
                            term.end_entity = chain.members.back().entity_id;
                            term.start_state = chain.start_state;
                            if (chain.filter) term.end_state = chain.filter->getState();
                            meta_nodes.push_back(std::move(term));
                            this_frame_entities.erase(chain.members.back().entity_id);
                        }
                    }

                    active_chains = std::move(remaining_chains);
                }
            }

            // Step 2: Start new chains for any remaining unused observations in current frame
            for (auto const & item: frame_lookup.at(f)) {
                EntityId entity_id = std::get<1>(item);
                auto used_key = std::make_pair(static_cast<long long>(f.getValue()), entity_id);
                if (used.count(used_key)) continue;

                DataType const * start_data = std::get<0>(item);

                // Initialize filter for this new chain
                FilterState start_state;
                FilterState updated_state;
                std::unique_ptr<IFilter> chain_filter;
                if (_filter_prototype) {
                    chain_filter = _filter_prototype->clone();
                    FilterState initial_state = _feature_extractor->getInitialState(*start_data);
                    chain_filter->initialize(initial_state);
                    start_state = chain_filter->getState();

                    // Immediately update filter with the first observation
                    // This ensures single-frame meta-nodes have correct end_state
                    Eigen::VectorXd obs = _feature_extractor->getFilterFeatures(*start_data);
                    updated_state = chain_filter->update(start_state, Measurement{obs});
                }

                // Start new active chain (defer meta-node until termination)
                this_frame_entities.erase(entity_id);
                used.insert(used_key);

                ActiveChain chain;
                chain.meta_node_idx = static_cast<size_t>(-1);
                chain.curr_frame = f;
                chain.curr_entity = entity_id;
                chain.curr_data = start_data;
                chain.filter = std::move(chain_filter);
                chain.members.push_back(NodeInfo{f, entity_id});
                chain.start_state = start_state;
                active_chains.push_back(std::move(chain));
            }

            if (this_frame_entities.size() != 0) {
                // ERROR WE LEFT A MAN BEHIND
                // Error out
                if (_logger) {
                    _logger->error("We left a man behind at frame {}", f.getValue(),
                                   " with entities: ", this_frame_entities.size());

                    for (auto const & entity: this_frame_entities) {
                        _logger->error("  Entity {}", entity);
                    }

                    // Is it in used?
                    for (auto const & entity: this_frame_entities) {
                        if (used.count(std::make_pair(static_cast<long long>(f.getValue()), entity))) {
                            _logger->error("  Entity {} is in used", entity);
                        }
                    }

                    // Is it in active_chains?
                    for (auto const & entity: this_frame_entities) {
                        for (auto const & chain: active_chains) {
                            if (chain.curr_entity == entity) {
                                _logger->error("  Entity {} is in active_chains", entity);
                                break;
                            }
                        }
                    }
                }
                throw std::runtime_error(
                        "We left a man behind at frame " + std::to_string(f.getValue()) +
                        " with entities: " + std::to_string(this_frame_entities.size()));
            }

            //Update progress every 1000 frames
            if (f.getValue() % 1000 == 0) {
                progress(static_cast<int>(f.getValue()) / (end_frame.getValue() - start_frame.getValue() + 1) * 100);
            }
        }

        // Finalize any remaining active chains at end of range
        for (auto & chain: active_chains) {
            MetaNode node;
            node.start_frame = chain.members.front().frame;
            node.start_entity = chain.members.front().entity_id;
            node.members = chain.members;
            node.end_frame = chain.members.back().frame;
            node.end_entity = chain.members.back().entity_id;
            node.start_state = chain.start_state;
            if (chain.filter) node.end_state = chain.filter->getState();
            if (_logger) {
                _logger->debug("Meta-node (finalized): frames {} to {} ({} frames), entities {} to {}, {} members - reached end",
                               node.start_frame.getValue(),
                               node.end_frame.getValue(),
                               node.end_frame.getValue() - node.start_frame.getValue() + 1,
                               node.start_entity,
                               node.end_entity,
                               node.members.size());
            }
            meta_nodes.push_back(std::move(node));
        }

        if (_logger) {
            _logger->debug("Built {} meta-nodes using Hungarian assignment", meta_nodes.size());

            // Compute statistics on meta-node lengths
            if (!meta_nodes.empty()) {
                std::vector<int> lengths;
                for (auto const & mn: meta_nodes) {
                    lengths.push_back(static_cast<int>(mn.members.size()));
                }
                std::sort(lengths.begin(), lengths.end());

                int total_length = 0;
                for (int len: lengths) total_length += len;
                double mean_length = static_cast<double>(total_length) / lengths.size();

                int median_length = lengths[lengths.size() / 2];
                int min_length = lengths.front();
                int max_length = lengths.back();

                _logger->debug("Meta-node length statistics: min={}, median={}, mean={:.1f}, max={}",
                               min_length, median_length, mean_length, max_length);

                // Count single-frame meta-nodes
                int single_frame_count = 0;
                for (int len: lengths) {
                    if (len == 1) single_frame_count++;
                }
                if (single_frame_count > 0) {
                    _logger->debug("  {} single-frame meta-nodes ({:.1f}%)",
                                   single_frame_count,
                                   100.0 * single_frame_count / meta_nodes.size());
                }
            }
        }
        return meta_nodes;
    }

    // Remove all the old greedy code that follows (everything from "double best_cost" to the old "return meta_nodes")
    /*
    OLD GREEDY CODE REMOVED - replaced with Hungarian-based approach above
    The new algorithm:
    1. Starts new chains for all unused observations at each frame
    2. Predicts all active chains forward one frame
    3. Builds cost matrix (chains x candidates)
    4. Uses Hungarian algorithm for optimal assignment
    5. Only accepts assignments below threshold
    6. Chains that don't get assigned (or exceed threshold) terminate
    
    This prevents "stealing" where long chains take candidates that would be better matches for other chains.
    */

    // --- N-Scan Lookahead Functions ---


    /**
     * @brief Detect if assignments are ambiguous at current frame.
     * Ambiguous if: (1) A chain has ≥2 candidates below threshold, OR
     *               (2) Multiple chains compete for the same candidate.
     * 
     * @param cost_matrix Cost matrix (chains x candidates)
     * @param threshold Assignment threshold
     * @return Indices of chains involved in ambiguous assignments
     */
    std::unordered_set<size_t> detect_ambiguous_chains(
            Eigen::MatrixXd const & cost_matrix,
            double threshold) const {

        std::unordered_set<size_t> ambiguous_chains;
        size_t num_chains = static_cast<size_t>(cost_matrix.rows());
        size_t num_candidates = static_cast<size_t>(cost_matrix.cols());

        // Check condition 1: chain has ≥2 candidates below threshold
        for (size_t chain_idx = 0; chain_idx < num_chains; ++chain_idx) {
            int count_below_threshold = 0;
            for (size_t cand_idx = 0; cand_idx < num_candidates; ++cand_idx) {
                if (cost_matrix(static_cast<Eigen::Index>(chain_idx),
                                static_cast<Eigen::Index>(cand_idx)) < threshold) {
                    ++count_below_threshold;
                }
            }
            if (count_below_threshold >= 2) {
                ambiguous_chains.insert(chain_idx);
            }
        }

        // Check condition 2: multiple chains compete for same candidate
        for (size_t cand_idx = 0; cand_idx < num_candidates; ++cand_idx) {
            int count_below_threshold = 0;
            size_t competing_chain = 0;
            for (size_t chain_idx = 0; chain_idx < num_chains; ++chain_idx) {
                if (cost_matrix(static_cast<Eigen::Index>(chain_idx),
                                static_cast<Eigen::Index>(cand_idx)) < threshold) {
                    ++count_below_threshold;
                    competing_chain = chain_idx;
                }
            }
            if (count_below_threshold >= 2) {
                // Mark all competing chains as ambiguous
                for (size_t chain_idx = 0; chain_idx < num_chains; ++chain_idx) {
                    if (cost_matrix(static_cast<Eigen::Index>(chain_idx),
                                    static_cast<Eigen::Index>(cand_idx)) < threshold) {
                        ambiguous_chains.insert(chain_idx);
                    }
                }
            }
        }

        return ambiguous_chains;
    }

    /**
     * @brief Expand hypotheses by one frame: predict, compute costs, and branch.
     * 
     * @param hypotheses Current hypotheses for a chain
     * @param candidates Available observations at next frame
     * @param next_frame Frame index of candidates
     * @param frame_lookup Frame data lookup
     * @param scoring_fn Function to compute total cost from frame costs
     * @return Updated hypotheses (terminated branches are marked)
     */
    std::vector<Hypothesis> expand_hypotheses(
            std::vector<Hypothesis> && hypotheses,
            std::vector<std::pair<EntityId, DataType const *>> const & candidates,
            TimeFrameIndex next_frame,
            std::map<TimeFrameIndex, FrameBucket<DataType>> const & frame_lookup,
            HypothesisScoringFunction const & scoring_fn) {

        std::vector<Hypothesis> expanded;

        for (auto & hyp: hypotheses) {
            if (hyp.terminated) {
                expanded.push_back(std::move(hyp));
                continue;
            }

            // Predict forward one frame
            FilterState predicted_state = hyp.filter->predict();

            if (_logger) {
                _logger->debug("      Expanding hyp (current_path_length={}): predicted_mean=[{:.2f},{:.2f}]",
                               hyp.path.size(), predicted_state.state_mean(0), predicted_state.state_mean(1));
            }

            // Try each candidate
            bool found_valid_branch = false;
            for (auto const & [cand_entity_id, cand_data]: candidates) {
                // Extract features
                Eigen::VectorXd measurement = _feature_extractor->getFilterFeatures(*cand_data);

                // Compute cost
                double cost = _lookahead_cost_function(predicted_state, measurement, 1);

                if (_logger && cost < _cheap_assignment_threshold) {
                    _logger->debug("        → entity {}: obs=[{:.2f},{:.2f}], cost={:.3f}",
                                   cand_entity_id, measurement(0), measurement(1), cost);
                }

                // Prune if exceeds lookahead threshold
                if (std::isfinite(_lookahead_threshold) && cost >= _lookahead_threshold) {
                    continue;
                }

                // Clone hypothesis and extend
                Hypothesis new_hyp;
                new_hyp.filter = hyp.filter->clone();
                new_hyp.current_state = new_hyp.filter->update(predicted_state, Measurement{measurement});
                new_hyp.path = hyp.path;
                new_hyp.path.push_back({next_frame, cand_entity_id});
                new_hyp.frame_costs = hyp.frame_costs;
                new_hyp.frame_costs.push_back(cost);
                new_hyp.total_cost = scoring_fn(new_hyp.frame_costs);
                new_hyp.terminated = false;

                expanded.push_back(std::move(new_hyp));
                found_valid_branch = true;
            }

            // If no valid branches, terminate this hypothesis
            if (!found_valid_branch) {
                hyp.terminated = true;
                expanded.push_back(std::move(hyp));
            }
        }

        return expanded;
    }

    /**
     * @brief Run N-scan lookahead for an ambiguous chain.
     * Explores multiple hypothesis paths over the next N frames and selects the best.
     * 
     * @param chain The active chain to run N-scan on
     * @param candidates_at_next_frame Initial candidates at the first lookahead frame
     * @param start_scan_frame The frame where N-scan starts
     * @param frame_lookup All frame data
     * @param used Set of already-used (frame, entity) pairs (will be updated)
     * @return Pair of (path, total_cost), or ({}, 0.0) if chain should terminate
     */
    std::pair<std::vector<NodeInfo>, double> run_n_scan_lookahead(
            ActiveChain const & chain,
            std::vector<std::tuple<EntityId, DataType const *, double>> const & candidates_with_costs,
            TimeFrameIndex start_scan_frame,
            TimeFrameIndex end_frame,
            std::map<TimeFrameIndex, FrameBucket<DataType>> const & frame_lookup,
            std::set<std::pair<long long, EntityId>> used// pass by value to make sure we don't modify the original set
    ) {

        // Early return if we can't scan ahead (at or near end frame)
        if (start_scan_frame + TimeFrameIndex(1) > end_frame) {
            if (_logger) {
                _logger->debug("N-scan skipped at frame {}: no future frames to scan", start_scan_frame.getValue());
            }
            return {{}, 0.0};
        }

        // Initialize hypotheses for each viable candidate
        std::vector<Hypothesis> hypotheses;
        for (auto const & [cand_entity, cand_data, cost_double]: candidates_with_costs) {
            if (cost_double >= _cheap_assignment_threshold) continue;

            Hypothesis hyp;
            hyp.filter = chain.filter ? chain.filter->clone() : nullptr;
            if (hyp.filter) {
                Eigen::VectorXd obs = _feature_extractor->getFilterFeatures(*cand_data);

                // Check if cloned filter state matches chain.predicted
                auto cloned_state = hyp.filter->getState();

                hyp.current_state = hyp.filter->update(chain.predicted, Measurement{obs});

                if (_logger) {
                    _logger->debug("    Init hyp for entity {}: chain.predicted=[{:.2f},{:.2f},{:.2f},{:.2f}], cloned_filter=[{:.2f},{:.2f},{:.2f},{:.2f}], obs=[{:.2f},{:.2f}], cost={:.3f}",
                                   cand_entity,
                                   chain.predicted.state_mean(0), chain.predicted.state_mean(1),
                                   chain.predicted.state_mean(2), chain.predicted.state_mean(3),
                                   cloned_state.state_mean(0), cloned_state.state_mean(1),
                                   cloned_state.state_mean(2), cloned_state.state_mean(3),
                                   obs(0), obs(1), cost_double);
                    _logger->debug("       After update: state=[{:.2f},{:.2f},{:.2f},{:.2f}]",
                                   hyp.current_state.state_mean(0), hyp.current_state.state_mean(1),
                                   hyp.current_state.state_mean(2), hyp.current_state.state_mean(3));
                }
            }
            hyp.path.push_back({start_scan_frame, cand_entity});
            hyp.frame_costs.push_back(cost_double);
            hyp.total_cost = score_hypothesis_simple_sum(hyp.frame_costs);
            hyp.terminated = false;

            hypotheses.push_back(std::move(hyp));
        }

        if (hypotheses.empty()) {
            return {{}, 0.0};// No viable paths
        }

        if (_logger) {
            _logger->debug("Starting N-scan at frame {} with {} initial hypotheses",
                           start_scan_frame.getValue(), hypotheses.size());
        }

        // Expand hypotheses over N frames
        for (int depth = 1; depth < _n_scan_depth; ++depth) {
            TimeFrameIndex scan_frame = start_scan_frame + TimeFrameIndex(depth);
            if (scan_frame > end_frame || !frame_lookup.count(scan_frame)) {
                break;// Reached end of available frames
            }

            // Collect available candidates
            std::vector<std::pair<EntityId, DataType const *>> scan_candidates;
            for (auto const & item: frame_lookup.at(scan_frame)) {
                EntityId cand_id = std::get<1>(item);
                auto key = std::make_pair(static_cast<long long>(scan_frame.getValue()), cand_id);
                if (used.count(key)) continue;

                scan_candidates.emplace_back(cand_id, std::get<0>(item));
            }

            if (scan_candidates.empty()) {
                break;// No candidates available
            }

            // Expand all hypotheses
            hypotheses = expand_hypotheses(std::move(hypotheses), scan_candidates, scan_frame,
                                           frame_lookup, score_hypothesis_simple_sum);

            // Check for early termination
            int viable_count = 0;
            for (auto const & hyp: hypotheses) {
                if (!hyp.terminated) viable_count++;
            }

            if (_logger) {
                _logger->debug("N-scan depth {}: {} viable hypotheses at frame {}",
                               depth, viable_count, scan_frame.getValue());
            }

            if (viable_count <= 1) {
                break;// Only one path remains, can commit early
            }
        }

        // Select best hypothesis
        bool reached_n = (hypotheses.empty() ? false : (hypotheses[0].path.size() >= static_cast<size_t>(_n_scan_depth)));
        auto best_hyp_opt = select_best_hypothesis(hypotheses, reached_n);

        if (!best_hyp_opt.has_value()) {
            if (_logger) {
                _logger->debug("N-scan terminated: ambiguity persists or no viable paths");
            }
            return {{}, 0.0};
        }

        // Mark used entities from the selected path
        auto const & best_path = best_hyp_opt->path;
        double best_cost = best_hyp_opt->total_cost;
        for (auto const & node: best_path) {
            used.insert(std::make_pair(static_cast<long long>(node.frame.getValue()), node.entity_id));
        }

        if (_logger) {
            _logger->debug("N-scan committed path with {} nodes, total cost {:.2f}",
                           best_path.size(), best_cost);
        }

        return {best_path, best_cost};
    }

    // --- Final Smoothing Step ---
    SmoothedResults generate_smoothed_results(
            std::map<GroupId, Path> const & solved_paths,
            std::map<TimeFrameIndex, FrameBucket<DataType>> const & frame_lookup,
            TimeFrameIndex start_frame,
            TimeFrameIndex end_frame) {

        SmoothedResults final_results;

        // Skip smoothing if no filter is provided
        if (!_filter_prototype) {
            return final_results;
        }

        for (auto const & [group_id, path]: solved_paths) {
            if (path.empty()) continue;

            auto filter = _filter_prototype->clone();
            std::vector<FilterState> forward_states;

            // Forward pass using the solved path
            for (size_t i = 0; i < path.size(); ++i) {
                auto const & node = path[i];
                auto const * data = findEntity(frame_lookup.at(node.frame), node.entity_id);
                if (!data) continue;

                if (i == 0) {
                    filter->initialize(_feature_extractor->getInitialState(*data));
                } else {
                    TimeFrameIndex prev_frame = path[i - 1].frame;
                    int num_steps = (node.frame - prev_frame).getValue();

                    if (num_steps <= 0) {
                        if (_logger) _logger->error("Invalid num_steps in smoothing: {}", num_steps);
                        continue;// Skip invalid steps
                    }

                    // Multi-step prediction: call predict() for each frame step
                    // The last predict() call will set the filter's internal state to the predicted state
                    FilterState pred = filter->getState();// Initialize with current state
                    for (int step = 0; step < num_steps; ++step) {
                        pred = filter->predict();
                    }
                    // Now filter's internal state is at 'pred', and we update it with the measurement
                    filter->update(pred, {_feature_extractor->getFilterFeatures(*data)});
                }
                forward_states.push_back(filter->getState());
            }

            // Backward smoothing pass
            if (forward_states.size() > 1) {
                final_results[group_id] = filter->smooth(forward_states);
            } else {
                final_results[group_id] = forward_states;
            }
        }
        return final_results;
    }

private:
    std::unique_ptr<IFilter> _filter_prototype;
    std::unique_ptr<IFeatureExtractor<DataType>> _feature_extractor;
    CostFunction _chain_cost_function;
    CostFunction _transition_cost_function;
    CostFunction _lookahead_cost_function;
    double _cost_scale_factor;
    double _cheap_assignment_threshold;
    std::shared_ptr<spdlog::logger> _logger;
    TrackerContractPolicy _policy = TrackerContractPolicy::Throw;
    TrackerDiagnostics _diagnostics{};
    int _n_scan_depth = 3;
    bool _enable_n_scan = true;
    int _max_gap_frames = 3;// Maximum frames to skip before terminating chain (-1 = unlimited)
    double _lookahead_threshold = std::numeric_limits<double>::infinity();
    double _ambiguity_threshold = 1.0;// default: stricter than cheap assignment
    double _ambiguity_margin = 0.0;   // default off
};

}// namespace StateEstimation

#endif// MIN_COST_FLOW_TRACKER_HPP
