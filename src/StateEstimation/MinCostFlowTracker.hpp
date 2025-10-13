#ifndef MIN_COST_FLOW_TRACKER_HPP
#define MIN_COST_FLOW_TRACKER_HPP

#include "Assignment/IAssigner.hpp"
#include "Assignment/hungarian.hpp"
#include "Cost/CostFunctions.hpp"
#include "DataSource.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Features/IFeatureExtractor.hpp"
#include "Filter/IFilter.hpp"
#include "Filter/Kalman/KalmanMatrixBuilder.hpp"
#include "MinCostFlowSolver.hpp"
#include "TimeFrame/TimeFrame.hpp"

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

namespace StateEstimation {

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
          _chain_cost_function(cost_function),
          _transition_cost_function(std::move(cost_function)),
          _cost_scale_factor(cost_scale_factor),
          _cheap_assignment_threshold(cheap_assignment_threshold) {}

    /**
     * @brief Construct with separate cost functions for greedy chaining and meta-node transitions.
     *
     * @param chain_cost_function Cost for frame-to-frame greedy chaining (typically 1-step)
     * @param transition_cost_function Cost for meta-node transitions across k-step gaps
     */
    MinCostFlowTracker(std::unique_ptr<IFilter> filter_prototype,
                       std::unique_ptr<IFeatureExtractor<DataType>> feature_extractor,
                       CostFunction chain_cost_function,
                       CostFunction transition_cost_function,
                       double cost_scale_factor,
                       double cheap_assignment_threshold)
        : _filter_prototype(std::move(filter_prototype)),
          _feature_extractor(std::move(feature_extractor)),
          _chain_cost_function(std::move(chain_cost_function)),
          _transition_cost_function(std::move(transition_cost_function)),
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
                    cost = _chain_cost_function(predicted, obs, 1);
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

        // Node indexing: 0..num_meta-1 are meta-nodes, plus source and sink
        int const source_node = num_meta;
        int const sink_node = num_meta + 1;

        // Build arcs for the abstract solver
        std::vector<ArcSpec> arcs;
        arcs.reserve(static_cast<size_t>(num_meta * num_meta / 4 + 4));
        // Source -> start meta
        arcs.push_back({source_node, *start_meta_opt, 1, 0});
        // End meta -> sink
        arcs.push_back({*end_meta_opt, sink_node, 1, 0});

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
                    // Coerce the saved end_state to the filter's expected state dimension if needed
                    FilterState const proto_state = temp_filter->getState();
                    int const target_dim = static_cast<int>(proto_state.state_mean.size());
                    FilterState init_state = from.end_state;
                    if (static_cast<int>(init_state.state_mean.size()) != target_dim ||
                        init_state.state_covariance.rows() != target_dim ||
                        init_state.state_covariance.cols() != target_dim) {
                        // Build a compatible state by copying what fits and padding the rest
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
                        // Pad remaining diagonal to a large uncertainty to remain conservative
                        constexpr double kPadVar = 1e6;
                        for (int d = 0; d < target_dim; ++d) {
                            if (coerced.state_covariance(d, d) <= 0.0) coerced.state_covariance(d, d) = kPadVar;
                        }
                        init_state = std::move(coerced);
                        if (_logger) {
                            _logger->warn("State dimension coerced for transition prediction: was {} -> now {}",
                                          static_cast<int>(from.end_state.state_mean.size()), target_dim);
                        }
                    }

                    temp_filter->initialize(init_state);
                    for (int s = 0; s < num_steps; ++s) {
                        predicted_state = temp_filter->predict();
                    }
                }
                // Compute cost to the first observation of 'to'
                DataType const * to_start_data = findEntity(frame_lookup.at(to.start_frame), to.start_entity);
                if (!to_start_data) continue;
                Eigen::VectorXd obs = _feature_extractor->getFilterFeatures(*to_start_data);
                double dist = _transition_cost_function(predicted_state, obs, num_steps);
                int64_t arc_cost = static_cast<int64_t>(dist * _cost_scale_factor);
                arcs.push_back({i, j, 1, arc_cost});
                num_transition_arcs++;
            }
        }

        if (_logger) {
            _logger->debug("Group {} meta-graph: {} meta-nodes, transitions={}",
                           static_cast<unsigned long long>(group_id), num_meta, num_transition_arcs);
        }

        // Solve using private solver and reconstruct meta-node path
        auto const seq_opt = solveMinCostSingleUnitPath(num_meta + 2, source_node, sink_node, arcs);
        if (!seq_opt.has_value()) {
            if (_logger) {
                _logger->error("Min-cost flow (meta) failed: no optimal path");
            }
            return {};
        }

        Path expanded_path;
        auto const & sequence = *seq_opt;
        for (size_t idx = 1; idx < sequence.size(); ++idx) {// skip the source at index 0
            int node_index = sequence[idx];
            if (node_index >= 0 && node_index < num_meta) {
                for (auto const & n: meta_nodes[static_cast<size_t>(node_index)].members) {
                    expanded_path.push_back(n);
                }
            }
        }

        return expanded_path;
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
            std::map<TimeFrameIndex, FrameBucket> const & frame_lookup,
            TimeFrameIndex start_frame,
            TimeFrameIndex end_frame,
            std::unordered_set<EntityId> const * excluded_entities,
            std::unordered_set<EntityId> const * include_entities) {

        // Structure to track active chains being built
        struct ActiveChain {
            size_t meta_node_idx;// Index in meta_nodes vector
            TimeFrameIndex curr_frame;
            EntityId curr_entity;
            DataType const * curr_data;
            std::unique_ptr<IFilter> filter;// Cloned filter for this chain
            FilterState predicted;          // Cached prediction for next frame

            // Constructor to properly initialize TimeFrameIndex
            ActiveChain()
                : meta_node_idx(0),
                  curr_frame(TimeFrameIndex(0)),
                  curr_entity(0),
                  curr_data(nullptr) {}
        };

        std::vector<MetaNode> meta_nodes;
        std::set<std::pair<long long, EntityId>> used;// key: (frame, entity)
        std::vector<ActiveChain> active_chains;

        // Process frame by frame, using Hungarian algorithm to extend chains optimally
        for (TimeFrameIndex f = start_frame; f <= end_frame; ++f) {
            if (!frame_lookup.count(f)) continue;

            // Step 1: Start new chains for unused observations in current frame
            for (auto const & item: frame_lookup.at(f)) {
                EntityId entity_id = std::get<1>(item);
                auto used_key = std::make_pair(static_cast<long long>(f.getValue()), entity_id);
                if (used.count(used_key)) continue;

                if (excluded_entities && excluded_entities->count(entity_id) > 0) {
                    if (!(include_entities && include_entities->count(entity_id) > 0)) {
                        continue;
                    }
                }

                DataType const * start_data = std::get<0>(item);

                // Initialize filter for this new chain
                FilterState start_state;
                std::unique_ptr<IFilter> chain_filter;
                if (_filter_prototype) {
                    chain_filter = _filter_prototype->clone();
                    chain_filter->initialize(_feature_extractor->getInitialState(*start_data));
                    start_state = chain_filter->getState();
                }

                // Create new meta-node
                MetaNode node;
                node.start_frame = f;
                node.start_entity = entity_id;
                node.members.push_back(NodeInfo{f, entity_id});
                node.start_state = start_state;
                // Seed end_state so it's never empty even for single-frame nodes
                node.end_state = start_state;
                used.insert(used_key);

                size_t node_idx = meta_nodes.size();
                meta_nodes.push_back(std::move(node));

                // Add to active chains for extension
                ActiveChain chain;
                chain.meta_node_idx = node_idx;
                chain.curr_frame = f;
                chain.curr_entity = entity_id;
                chain.curr_data = start_data;
                chain.filter = std::move(chain_filter);
                active_chains.push_back(std::move(chain));
            }

            // Step 2: Try to extend all active chains to next frame using Hungarian algorithm
            TimeFrameIndex next_frame = f + TimeFrameIndex(1);
            if (next_frame > end_frame || !frame_lookup.count(next_frame)) {
                continue;// No next frame, chains will terminate
            }

            if (active_chains.empty()) continue;

            // Predict all active chains
            for (auto & chain: active_chains) {
                if (chain.filter) {
                    chain.predicted = chain.filter->predict();
                }
            }

            // Collect available candidates in next frame
            std::vector<std::tuple<EntityId, DataType const *, size_t>> candidates;// entity_id, data, index
            for (size_t cand_idx = 0; cand_idx < frame_lookup.at(next_frame).size(); ++cand_idx) {
                auto const & cand = frame_lookup.at(next_frame)[cand_idx];
                EntityId cand_id = std::get<1>(cand);
                auto key = std::make_pair(static_cast<long long>(next_frame.getValue()), cand_id);
                if (used.count(key)) continue;
                if (excluded_entities && excluded_entities->count(cand_id) > 0) {
                    if (!(include_entities && include_entities->count(cand_id) > 0)) {
                        continue;
                    }
                }
                DataType const * cand_data = std::get<0>(cand);
                candidates.emplace_back(cand_id, cand_data, cand_idx);
            }

            if (candidates.empty()) {
                // No candidates available - all chains terminate
                if (_logger) {
                    _logger->debug("{} active chains terminating at frame {} (no available candidates in frame {})",
                                   active_chains.size(), f.getValue(), next_frame.getValue());
                }
                active_chains.clear();
                continue;
            }

            // Build cost matrix for Hungarian algorithm
            // Rows = active chains, Cols = candidates
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
                        cost_double = _chain_cost_function(chain.predicted, obs, 1);
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

            // Process assignments
            std::vector<bool> chain_extended(active_chains.size(), false);
            std::vector<ActiveChain> remaining_chains;

            for (size_t chain_idx = 0; chain_idx < assignment_matrix.size(); ++chain_idx) {
                bool found_assignment = false;
                int assigned_cand_idx = -1;

                for (size_t cand_idx = 0; cand_idx < assignment_matrix[chain_idx].size(); ++cand_idx) {
                    if (assignment_matrix[chain_idx][cand_idx] == 1) {
                        // Check if cost is within threshold
                        if (cost_matrix[chain_idx][cand_idx] <= max_cost) {
                            found_assignment = true;
                            assigned_cand_idx = static_cast<int>(cand_idx);
                        }
                        break;
                    }
                }

                auto & chain = active_chains[chain_idx];
                auto & node = meta_nodes[chain.meta_node_idx];

                if (found_assignment) {
                    // Extend chain
                    EntityId best_entity = std::get<0>(candidates[assigned_cand_idx]);
                    DataType const * best_data = std::get<1>(candidates[assigned_cand_idx]);

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

                            _logger->warn("State covariance singular at frame {} entity {}: det={:.2e}, cond={:.2e}",
                                          next_frame.getValue(), best_entity, determinant, condition_number);

                            if (condition_number > 1e12) {
                                _logger->warn("  Terminating chain due to ill-conditioned covariance");
                                found_assignment = false;// Terminate this chain
                            }
                        }
                    }

                    if (found_assignment) {
                        NodeInfo next_info{next_frame, best_entity};
                        node.members.push_back(next_info);
                        used.insert(std::make_pair(static_cast<long long>(next_frame.getValue()), best_entity));

                        chain.curr_frame = next_frame;
                        chain.curr_entity = best_entity;
                        chain.curr_data = best_data;
                        chain_extended[chain_idx] = true;
                        remaining_chains.push_back(std::move(chain));
                    }
                }

                if (!found_assignment) {
                    // Chain terminates - finalize meta-node
                    node.end_frame = node.members.back().frame;
                    node.end_entity = node.members.back().entity_id;
                    if (chain.filter) {
                        node.end_state = chain.filter->getState();
                    }

                    if (_logger) {
                        // Log why chain ended
                        double best_cost_double = std::numeric_limits<double>::infinity();
                        if (assigned_cand_idx >= 0) {
                            best_cost_double = cost_matrix[chain_idx][assigned_cand_idx] / static_cast<double>(cost_scaling_factor);
                        }

                        _logger->debug("Meta-node #{}: frames {} to {} ({} frames), entities {} to {}, {} members - terminated (best_cost={:.2f}, threshold={:.2f})",
                                       chain.meta_node_idx,
                                       node.start_frame.getValue(),
                                       node.end_frame.getValue(),
                                       node.end_frame.getValue() - node.start_frame.getValue() + 1,
                                       node.start_entity,
                                       node.end_entity,
                                       node.members.size(),
                                       best_cost_double,
                                       _cheap_assignment_threshold);
                    }
                }
            }

            active_chains = std::move(remaining_chains);
        }

        // Finalize any remaining active chains at end of range
        for (auto & chain: active_chains) {
            auto & node = meta_nodes[chain.meta_node_idx];
            node.end_frame = node.members.back().frame;
            node.end_entity = node.members.back().entity_id;
            if (chain.filter) {
                node.end_state = chain.filter->getState();
            }

            if (_logger) {
                _logger->debug("Meta-node #{}: frames {} to {} ({} frames), entities {} to {}, {} members - reached end",
                               chain.meta_node_idx,
                               node.start_frame.getValue(),
                               node.end_frame.getValue(),
                               node.end_frame.getValue() - node.start_frame.getValue() + 1,
                               node.start_entity,
                               node.end_entity,
                               node.members.size());
            }
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
    CostFunction _chain_cost_function;
    CostFunction _transition_cost_function;
    double _cost_scale_factor;
    double _cheap_assignment_threshold;
    std::shared_ptr<spdlog::logger> _logger;
};

}// namespace StateEstimation

#endif// MIN_COST_FLOW_TRACKER_HPP
