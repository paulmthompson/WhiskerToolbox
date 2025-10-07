#ifndef MIN_COST_FLOW_TRACKER_HPP
#define MIN_COST_FLOW_TRACKER_HPP

#include "Assignment/IAssigner.hpp"
#include "DataSource.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Features/IFeatureExtractor.hpp"
#include "Filter/IFilter.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <Eigen/Dense>

#include "ortools/graph/min_cost_flow.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"

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

namespace StateEstimation {

/**
 * @brief Smoothed results for each group over frames.
 */
using SmoothedResults = std::map<GroupId, std::vector<FilterState>>;

/**
 * @brief Progress callback: percent complete [0,100].
 */
using ProgressCallback = std::function<void(int)>;

/**
 * @brief Cost function signature: computes cost between predicted state and observation.
 * 
 * @param predicted_state The predicted state from the filter
 * @param observation The observation feature vector
 * @param num_gap_frames Number of frames in the gap (for gap-dependent costs)
 * @return Cost value (non-negative)
 */
using CostFunction = std::function<double(FilterState const &, Eigen::VectorXd const &, int)>;

/**
 * @brief Factory function to create a Mahalanobis distance-based cost function.
 * 
 * This is the default cost function that computes the Mahalanobis distance between
 * the predicted measurement (H * predicted_state) and the actual observation.
 * 
 * @param H Measurement matrix (maps state space to measurement space)
 * @param R Measurement noise covariance matrix
 * @return CostFunction that computes Mahalanobis distance
 */
inline CostFunction createMahalanobisCostFunction(Eigen::MatrixXd const & H,
                                                  Eigen::MatrixXd const & R) {
    return [H, R](FilterState const & predicted_state,
                  Eigen::VectorXd const & observation,
                  int /* num_gap_frames */) -> double {
        Eigen::VectorXd innovation = observation - (H * predicted_state.state_mean);
        Eigen::MatrixXd innovation_covariance = H * predicted_state.state_covariance * H.transpose() + R;
        double dist_sq = innovation.transpose() * innovation_covariance.inverse() * innovation;
        return std::sqrt(dist_sq);
    };
}

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
    using GroundTruthMap = std::map<TimeFrameIndex, std::map<GroupId, EntityId>>;
    using FrameBucket = std::vector<std::tuple<DataType const *, EntityId, TimeFrameIndex>>;

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
     */
    MinCostFlowTracker(std::unique_ptr<IFilter> filter_prototype,
                       std::unique_ptr<IFeatureExtractor<DataType>> feature_extractor,
                       CostFunction cost_function,
                       double cost_scale_factor = 100.0,
                       double cheap_assignment_threshold = 5.0)
        : _filter_prototype(std::move(filter_prototype)),
          _feature_extractor(std::move(feature_extractor)),
          _cost_function(std::move(cost_function)),
          _cost_scale_factor(cost_scale_factor),
          _cheap_assignment_threshold(cheap_assignment_threshold) {}

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
     */
    MinCostFlowTracker(std::unique_ptr<IFilter> filter_prototype,
                       std::unique_ptr<IFeatureExtractor<DataType>> feature_extractor,
                       Eigen::MatrixXd const & measurement_matrix,
                       Eigen::MatrixXd const & measurement_noise_covariance,
                       double cost_scale_factor = 100.0,
                       double cheap_assignment_threshold = 5.0)
        : MinCostFlowTracker(std::move(filter_prototype),
                             std::move(feature_extractor),
                             createMahalanobisCostFunction(measurement_matrix, measurement_noise_covariance),
                             cost_scale_factor,
                             cheap_assignment_threshold) {}

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
                                          ProgressCallback progress = nullptr,
                                          std::map<GroupId, GroupId> const * output_group_ids = nullptr,
                                          std::unordered_set<EntityId> const * excluded_entities = nullptr,
                                          std::unordered_set<EntityId> const * include_entities = nullptr) {
        if (_logger) {
            _logger->debug("MCF process: start={} end={}", start_frame.getValue(), end_frame.getValue());
        }

        auto frame_lookup = buildFrameLookup(data_source, start_frame, end_frame);
        auto start_anchors_it = ground_truth.find(start_frame);
        auto end_anchors_it = ground_truth.find(end_frame);

        if (start_anchors_it == ground_truth.end() || end_anchors_it == ground_truth.end()) {
            if (_logger) _logger->error("Min-cost flow requires anchors at both start and end frames.");
            return {};
        }

        // 1. --- Build and Solve the Graph ---
        auto solved_paths = solve_flow_problem(frame_lookup,
                                               start_anchors_it->second,
                                               end_anchors_it->second,
                                               start_frame,
                                               end_frame,
                                               excluded_entities,
                                               include_entities);

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

private:
    struct NodeInfo {
        TimeFrameIndex frame = TimeFrameIndex(0);
        EntityId entity_id = 0;

        bool operator<(NodeInfo const & other) const {
            if (frame != other.frame) return frame < other.frame;
            return entity_id < other.entity_id;
        }
    };

    using Path = std::vector<NodeInfo>;

    /**
     * @brief Represents a greedy-linked sequence (meta-node) of cheap assignments across consecutive frames.
     * 
     * Each meta-node aggregates observations that are very likely to belong to the same chain, so that
     * min-cost flow can operate sparsely on these chains instead of per-observation nodes.
     */
    struct MetaNode {
        std::vector<NodeInfo> members;// consecutive observations included in this chain
        FilterState start_state;      // filter state after initializing on first observation
        FilterState end_state;        // filter state after updating on last observation
        TimeFrameIndex start_frame = TimeFrameIndex(0);
        TimeFrameIndex end_frame = TimeFrameIndex(0);
        EntityId start_entity = 0;
        EntityId end_entity = 0;
    };

    // Arc metadata: stores the actual chain of entities represented by this arc
    struct ArcChain {
        std::vector<NodeInfo> entities;// All entities along this arc (including endpoints)
        int64_t cost;
    };

    // Helper: Build a chain of entities from start_node to end_node by greedily following best matches
    // Returns the chain and accumulated cost
    ArcChain build_entity_chain(
            NodeInfo const & start_node,
            NodeInfo const & end_node,
            std::map<TimeFrameIndex, FrameBucket> const & frame_lookup) {

        ArcChain chain;
        chain.entities.push_back(start_node);
        chain.cost = 0;

        // If start and end are consecutive frames or same frame, just connect directly
        if (end_node.frame <= start_node.frame + TimeFrameIndex(1)) {
            if (end_node.frame == start_node.frame + TimeFrameIndex(1)) {
                chain.entities.push_back(end_node);
            }
            return chain;
        }

        // Build chain frame-by-frame using greedy best match
        TimeFrameIndex current_frame = start_node.frame;
        EntityId current_entity = start_node.entity_id;

        while (current_frame + TimeFrameIndex(1) < end_node.frame) {
            TimeFrameIndex next_frame = current_frame + TimeFrameIndex(1);

            // Skip frames with no observations
            if (!frame_lookup.count(next_frame)) {
                current_frame = next_frame;
                continue;
            }

            // Get current entity data
            DataType const * current_data = nullptr;
            if (frame_lookup.count(current_frame)) {
                for (auto const & item: frame_lookup.at(current_frame)) {
                    if (std::get<1>(item) == current_entity) {
                        current_data = std::get<0>(item);
                        break;
                    }
                }
            }

            if (!current_data) break;// Can't continue chain

            // Find best match at next frame
            double best_cost = std::numeric_limits<double>::max();
            EntityId best_entity = 0;

            for (auto const & candidate: frame_lookup.at(next_frame)) {
                EntityId candidate_id = std::get<1>(candidate);
                DataType const * candidate_data = std::get<0>(candidate);

                double cost = 0.0;
                if (_filter_prototype) {
                    auto temp_filter = _filter_prototype->clone();
                    temp_filter->initialize(_feature_extractor->getInitialState(*current_data));
                    FilterState predicted = temp_filter->predict();
                    Eigen::VectorXd obs = _feature_extractor->getFilterFeatures(*candidate_data);
                    cost = _cost_function(predicted, obs, 1);
                } else {
                    // Simple distance without filter
                    Eigen::VectorXd feat_current = _feature_extractor->getFilterFeatures(*current_data);
                    Eigen::VectorXd feat_candidate = _feature_extractor->getFilterFeatures(*candidate_data);
                    cost = (feat_current - feat_candidate).norm();
                }

                if (cost < best_cost) {
                    best_cost = cost;
                    best_entity = candidate_id;
                }
            }

            if (best_cost < std::numeric_limits<double>::max()) {
                chain.entities.push_back({next_frame, best_entity});
                chain.cost += static_cast<int64_t>(best_cost * _cost_scale_factor);
                current_entity = best_entity;
                current_frame = next_frame;
            } else {
                break;// No valid match found
            }
        }

        // Add end node
        chain.entities.push_back(end_node);

        return chain;
    }

    // --- Main Graph Building and Solving Logic ---
    std::map<GroupId, Path> solve_flow_problem(
            std::map<TimeFrameIndex, FrameBucket> const & frame_lookup,
            std::map<GroupId, EntityId> const & start_anchors,
            std::map<GroupId, EntityId> const & end_anchors,
            TimeFrameIndex start_frame,
            TimeFrameIndex end_frame,
            std::unordered_set<EntityId> const * excluded_entities,
            std::unordered_set<EntityId> const * include_entities) {

        // 1) Build greedy meta-nodes (cheap consecutive links) independent of groups
        auto meta_nodes = build_meta_nodes(frame_lookup, start_frame, end_frame, excluded_entities, include_entities);

        std::map<GroupId, Path> all_solved_paths;

        // 2) Solve a separate min-cost flow problem for each group over meta-nodes
        for (auto const & [group_id, start_entity_id]: start_anchors) {
            auto end_anchors_it = end_anchors.find(group_id);
            if (end_anchors_it == end_anchors.end()) {
                if (_logger) {
                    _logger->error("No end anchor found for group {}", static_cast<unsigned long long>(group_id));
                }
                continue;
            }
            EntityId end_entity_id = end_anchors_it->second;

            Path solved_path = solve_single_group_flow_over_meta(meta_nodes, frame_lookup, group_id, start_entity_id, end_entity_id, start_frame, end_frame);
            if (!solved_path.empty()) {
                all_solved_paths[group_id] = solved_path;
            }
        }

        return all_solved_paths;
    }

    // Solve min-cost flow for a single group over meta-nodes
    Path solve_single_group_flow_over_meta(
            std::vector<MetaNode> const & meta_nodes,
            std::map<TimeFrameIndex, FrameBucket> const & frame_lookup,
            GroupId group_id,
            EntityId start_entity_id,
            EntityId end_entity_id,
            TimeFrameIndex start_frame,
            TimeFrameIndex end_frame) {

        // Map each meta-node to an index
        int const num_meta = static_cast<int>(meta_nodes.size());
        auto get_start_meta_index = [&]() -> std::optional<int> {
            for (int i = 0; i < num_meta; ++i) {
                if (meta_nodes[i].start_frame == start_frame && meta_nodes[i].start_entity == start_entity_id) {
                    return i;
                }
            }
            return std::nullopt;
        };
        auto get_end_meta_index = [&]() -> std::optional<int> {
            for (int i = 0; i < num_meta; ++i) {
                // End meta-node must contain the end anchor entity at end_frame
                if (meta_nodes[i].end_frame == end_frame && meta_nodes[i].end_entity == end_entity_id) {
                    return i;
                }
            }
            return std::nullopt;
        };

        auto start_meta_opt = get_start_meta_index();
        auto end_meta_opt = get_end_meta_index();
        if (!start_meta_opt.has_value() || !end_meta_opt.has_value()) {
            if (_logger) {
                _logger->error("Group {} missing start or end meta-node anchor (start={}, end={})",
                               static_cast<unsigned long long>(group_id),
                               start_meta_opt.has_value(), end_meta_opt.has_value());
            }
            return {};
        }

        operations_research::SimpleMinCostFlow min_cost_flow;
        // Node indexing: 0..num_meta-1 are meta-nodes, plus source and sink
        int const source_node = num_meta;
        int const sink_node = num_meta + 1;

        // Build arcs
        // Source -> start meta
        min_cost_flow.AddArcWithCapacityAndUnitCost(source_node, *start_meta_opt, 1, 0);
        // End meta -> sink
        min_cost_flow.AddArcWithCapacityAndUnitCost(*end_meta_opt, sink_node, 1, 0);

        // Transitions between meta-nodes (only forward in time)
        int num_transition_arcs = 0;
        constexpr int64_t max_prediction_horizon = 50;// allow longer jumps across blackouts
        for (int i = 0; i < num_meta; ++i) {
            MetaNode const & from = meta_nodes[i];
            for (int j = 0; j < num_meta; ++j) {
                MetaNode const & to = meta_nodes[j];
                if (to.start_frame <= from.end_frame) continue;// must go forward
                int num_steps = (to.start_frame - from.end_frame).getValue();
                if (num_steps <= 0 || num_steps > max_prediction_horizon) continue;

                // Predict from the end of 'from' to the start of 'to'
                FilterState predicted_state;
                if (_filter_prototype) {
                    auto temp_filter = _filter_prototype->clone();
                    temp_filter->initialize(from.end_state);
                    for (int s = 0; s < num_steps; ++s) {
                        predicted_state = temp_filter->predict();
                    }
                }
                // Compute cost to the first observation of 'to'
                DataType const * to_start_data = findEntity(frame_lookup.at(to.start_frame), to.start_entity);
                if (!to_start_data) continue;
                Eigen::VectorXd obs = _feature_extractor->getFilterFeatures(*to_start_data);
                double dist = _cost_function(predicted_state, obs, num_steps);
                int64_t arc_cost = static_cast<int64_t>(dist * _cost_scale_factor);
                min_cost_flow.AddArcWithCapacityAndUnitCost(i, j, 1, arc_cost);
                num_transition_arcs++;
            }
        }

        if (_logger) {
            _logger->debug("Group {} meta-graph: {} meta-nodes, transitions={}",
                           static_cast<unsigned long long>(group_id), num_meta, num_transition_arcs);
        }

        // Supplies
        min_cost_flow.SetNodeSupply(source_node, 1);
        min_cost_flow.SetNodeSupply(sink_node, -1);

        auto solve_status = min_cost_flow.Solve();
        if (solve_status != operations_research::SimpleMinCostFlow::OPTIMAL) {
            if (_logger) {
                _logger->error("Min-cost flow (meta) failed with status: {}", static_cast<int>(solve_status));
            }
            return {};
        }

        // Reconstruct meta-node path
        std::map<int, int> succ;
        for (int a = 0; a < min_cost_flow.NumArcs(); ++a) {
            if (min_cost_flow.Flow(a) > 0) {
                int tail = min_cost_flow.Tail(a);
                int head = min_cost_flow.Head(a);
                succ[tail] = head;
            }
        }

        Path expanded_path;
        int current = source_node;
        std::unordered_set<int> visited;
        while (succ.find(current) != succ.end() && current != sink_node) {
            int next = succ[current];
            if (next >= 0 && next < num_meta) {
                // Append all members of this meta-node
                for (auto const & n: meta_nodes[next].members) {
                    expanded_path.push_back(n);
                }
            }
            if (visited.count(next)) break;// safety against unexpected cycles
            visited.insert(next);
            current = next;
        }

        return expanded_path;
    }

    /**
     * @brief Build meta-nodes by greedily linking consecutive cheap assignments with a Kalman filter.
     * 
     * @pre frame_lookup contains observations in [start_frame, end_frame]
     * @post Each observation belongs to at most one meta-node
     */
    std::vector<MetaNode> build_meta_nodes(
            std::map<TimeFrameIndex, FrameBucket> const & frame_lookup,
            TimeFrameIndex start_frame,
            TimeFrameIndex end_frame,
            std::unordered_set<EntityId> const * excluded_entities,
            std::unordered_set<EntityId> const * include_entities) {
        std::vector<MetaNode> meta_nodes;
        std::set<std::pair<long long, EntityId>> used;// key: (frame, entity)

        for (TimeFrameIndex f = start_frame; f <= end_frame; ++f) {
            if (!frame_lookup.count(f)) continue;
            for (auto const & item: frame_lookup.at(f)) {
                NodeInfo start_info{f, std::get<1>(item)};
                auto used_key = std::make_pair(static_cast<long long>(f.getValue()), start_info.entity_id);
                if (used.count(used_key)) continue;
                if (excluded_entities && excluded_entities->count(start_info.entity_id) > 0) {
                    // Allow included exceptions (e.g., anchors)
                    if (!(include_entities && include_entities->count(start_info.entity_id) > 0)) {
                        continue;
                    }
                }

                DataType const * start_data = std::get<0>(item);
                // Initialize filter for this potential chain
                FilterState start_state;
                std::unique_ptr<IFilter> chain_filter;
                if (_filter_prototype) {
                    chain_filter = _filter_prototype->clone();
                    chain_filter->initialize(_feature_extractor->getInitialState(*start_data));
                    start_state = chain_filter->getState();
                }

                MetaNode node;
                node.start_frame = f;
                node.start_entity = start_info.entity_id;
                node.members.push_back(start_info);
                node.start_state = start_state;
                used.insert(used_key);

                // Try to greedily extend to next frames while cheap
                TimeFrameIndex curr_frame = f;
                EntityId curr_entity = start_info.entity_id;
                DataType const * curr_data = start_data;
                while (curr_frame + TimeFrameIndex(1) <= end_frame) {
                    TimeFrameIndex next_frame = curr_frame + TimeFrameIndex(1);
                    if (!frame_lookup.count(next_frame)) break;// no data: stop chain here

                    // Find best cheap match in next frame among unused
                    double best_cost = std::numeric_limits<double>::infinity();
                    EntityId best_entity = 0;
                    DataType const * best_data = nullptr;
                    FilterState predicted;
                    if (chain_filter) {
                        predicted = chain_filter->predict();
                    }

                    for (auto const & cand: frame_lookup.at(next_frame)) {
                        EntityId cand_id = std::get<1>(cand);
                        auto key = std::make_pair(static_cast<long long>(next_frame.getValue()), cand_id);
                        if (used.count(key)) continue;
                        if (excluded_entities && excluded_entities->count(cand_id) > 0) {
                            if (!(include_entities && include_entities->count(cand_id) > 0)) {
                                continue;
                            }
                        }
                        DataType const * cand_data = std::get<0>(cand);
                        Eigen::VectorXd obs = _feature_extractor->getFilterFeatures(*cand_data);
                        double cost = _filter_prototype ? _cost_function(predicted, obs, 1)
                                                        : (_feature_extractor->getFilterFeatures(*curr_data) - obs).norm();
                        if (cost < best_cost) {
                            best_cost = cost;
                            best_entity = cand_id;
                            best_data = cand_data;
                        }
                    }

                    if (best_data && best_cost <= _cheap_assignment_threshold) {
                        // Accept and update
                        Eigen::VectorXd obs = _feature_extractor->getFilterFeatures(*best_data);
                        if (chain_filter) {
                            chain_filter->update(predicted, {obs});
                        }
                        NodeInfo next_info{next_frame, best_entity};
                        node.members.push_back(next_info);
                        used.insert(std::make_pair(static_cast<long long>(next_frame.getValue()), best_entity));
                        curr_frame = next_frame;
                        curr_entity = best_entity;
                        curr_data = best_data;
                    } else {
                        break;// cannot extend cheaply
                    }
                }

                // Finalize meta-node
                node.end_frame = node.members.back().frame;
                node.end_entity = node.members.back().entity_id;
                if (chain_filter) {
                    node.end_state = chain_filter->getState();
                }
                meta_nodes.push_back(std::move(node));
            }
        }

        if (_logger) {
            _logger->debug("Built {} meta-nodes (cheap chains)", meta_nodes.size());
        }
        return meta_nodes;
    }

    // --- Final Smoothing Step ---
    SmoothedResults generate_smoothed_results(
            std::map<GroupId, Path> const & solved_paths,
            std::map<TimeFrameIndex, FrameBucket> const & frame_lookup,
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

    // --- Utility Functions ---
    [[nodiscard]] std::map<TimeFrameIndex, FrameBucket>
    buildFrameLookup(auto && data_source, TimeFrameIndex start_frame, TimeFrameIndex end_frame) const {
        std::map<TimeFrameIndex, FrameBucket> lookup;
        for (auto const & item: data_source) {
            TimeFrameIndex t = getTimeFrameIndex(item);
            if (t >= start_frame && t <= end_frame) {
                lookup[t].emplace_back(&getData(item), getEntityId(item), t);
            }
        }
        return lookup;
    }

    static DataType const * findEntity(FrameBucket const & bucket, EntityId id) {
        for (auto const & item: bucket) {
            if (std::get<1>(item) == id) return std::get<0>(item);
        }
        return nullptr;
    }

private:
    std::unique_ptr<IFilter> _filter_prototype;
    std::unique_ptr<IFeatureExtractor<DataType>> _feature_extractor;
    CostFunction _cost_function;
    double _cost_scale_factor;
    double _cheap_assignment_threshold;
    std::shared_ptr<spdlog::logger> _logger;
};

}// namespace StateEstimation

#endif// MIN_COST_FLOW_TRACKER_HPP
