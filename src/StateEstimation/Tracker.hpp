#ifndef TRACKER_NEW_HPP
#define TRACKER_NEW_HPP

#include "Assignment/IAssigner.hpp"
#include "DataSource.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Features/IFeatureExtractor.hpp"
#include "Filter/IFilter.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <cmath>
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
 * @brief Internal per-group tracking buffers for a single interval between anchors.
 */
struct IntervalHistory {
    std::vector<TimeFrameIndex> frames;
    std::vector<FilterState> forward_states;
    std::vector<FilterState> forward_predictions;
    std::vector<std::optional<EntityId>> assigned_entities;
};

/**
 * @brief The new tracker implementing iterative RTS smoothing without dynamic measurement noise.
 *
 * - No identity-dependent measurement noise scaling (fixed R in the filter)
 * - No backward Kalman filter; use standard Rauch–Tung–Striebel smoothing
 * - Iteratively refine: forward pass with assignment -> RTS smoothing -> re-assignment on smoothed
 *   predictions, until assignments at anchors match ground truth or max iterations reached.
 *
 * @tparam DataType raw observation type (e.g., Line2D)
 */
template<typename DataType>
class Tracker {
public:
    using GroundTruthMap = std::map<TimeFrameIndex, std::map<GroupId, EntityId>>;

    /**
     * @brief Construct a new TrackerNew
     *
     * @param filter_prototype Prototype filter (cloned per group)
     * @param feature_extractor Feature extractor for DataType
     * @param assigner Assigner used for data-to-group association
     * @pre All pointers are valid
     */
    Tracker(std::unique_ptr<IFilter> filter_prototype,
            std::unique_ptr<IFeatureExtractor<DataType>> feature_extractor,
            std::unique_ptr<IAssigner> assigner)
        : _filter_prototype(std::move(filter_prototype)),
          _feature_extractor(std::move(feature_extractor)),
          _assigner(std::move(assigner)) {}

    /**
     * @brief Process a range of frames with iterative RTS smoothing until consistency.
     *
     * @tparam Source Range of tuples (DataType, EntityId, TimeFrameIndex)
     * @param data_source Zero-copy data source
     * @param group_manager Group manager to read existing groups and record assignments
     * @param ground_truth Ground truth at specific frames for certain groups (anchors)
     * @param start_frame Inclusive start frame
     * @param end_frame Inclusive end frame
     * @param max_iterations Maximum smoothing/assignment refinement iterations
     * @param progress Optional progress callback
     * @return Smoothed states per group across processed frames
     *
     * @pre start_frame <= end_frame
     * @post Returned results contain per-group smoothed states aligned with processed frames
     */
    template<typename Source>
        requires DataSource<Source, DataType>
    [[nodiscard]] SmoothedResults process(Source && data_source,
                                          EntityGroupManager & group_manager,
                                          GroundTruthMap const & ground_truth,
                                          TimeFrameIndex start_frame,
                                          TimeFrameIndex end_frame,
                                          int max_iterations,
                                          ProgressCallback progress = nullptr) {
        auto frame_lookup = buildFrameLookup(data_source, start_frame, end_frame);
        if (_logger) {
            _logger->debug("process: start={} end={} max_iters={}", start_frame.getValue(), end_frame.getValue(), max_iterations);
        }

        // Initialize per-group filters
        for (auto group_id: group_manager.getAllGroupIds()) {
            if (_tracks.find(group_id) == _tracks.end()) {
                _tracks[group_id] = std::make_unique<GroupTrack>();
                _tracks[group_id]->_filter = _filter_prototype->clone();
                _tracks[group_id]->_active = false;
            }
        }

        // Iterate: forward pass -> RTS smoothing -> reassign -> check anchors
        SmoothedResults final_results;
        std::optional<std::map<GroupId, IntervalHistory>> prev_smoothed_histories;
        for (int iter = 0; iter < max_iterations; ++iter) {
            if (_logger) {
                _logger->debug("iteration {} begin", iter);
            }
            _pending_updates.clear();
            _planned_assignments.clear();
            _anchor_mismatches.clear();
            auto forward_histories = runForwardPass(frame_lookup, group_manager, ground_truth, start_frame, end_frame, progress, prev_smoothed_histories);
            auto smoothed_histories = runRtsSmoothing(forward_histories);

            // Snap smoothed predictions to nearest observations (respecting anchors) for next iteration
            buildSnappedAssignments(smoothed_histories, frame_lookup, ground_truth);

            // --- Iteration metrics logging ---
            if (_logger) {
                for (auto const & [gid, hist]: smoothed_histories) {
                    double sum_pos_delta = 0.0;
                    double max_pos_delta = 0.0;
                    size_t n = hist.forward_states.size();
                    for (size_t i = 0; i < n; ++i) {
                        // Baseline: previous smoothed if available; otherwise this iteration's forward state
                        FilterState const & baseline = prev_smoothed_histories && prev_smoothed_histories->count(gid) && (*prev_smoothed_histories)[gid].forward_states.size() == n
                                                       ? (*prev_smoothed_histories)[gid].forward_states[i]
                                                       : forward_histories[gid].forward_states[i];
                        auto const & cur = hist.forward_states[i];
                        double dx = cur.state_mean(0) - baseline.state_mean(0);
                        double dy = cur.state_mean(1) - baseline.state_mean(1);
                        double d = std::hypot(dx, dy);
                        sum_pos_delta += d;
                        if (d > max_pos_delta) max_pos_delta = d;
                    }
                    double mean_pos_delta = n > 0 ? sum_pos_delta / static_cast<double>(n) : 0.0;

                    // Count snapped vs forward differences
                    size_t snapped_changes = 0;
                    for (size_t i = 0; i < hist.frames.size(); ++i) {
                        auto f = hist.frames[i];
                        auto it_pl = _planned_assignments.find(gid);
                        if (it_pl != _planned_assignments.end()) {
                            auto itf = it_pl->second.find(f);
                            if (itf != it_pl->second.end()) {
                                auto const & prev_assigned = forward_histories[gid].assigned_entities[i];
                                if (!prev_assigned.has_value() || itf->second != *prev_assigned) {
                                    snapped_changes++;
                                }
                            }
                        }
                    }

                    _logger->debug("iter {} metrics: group={} frames={} mean_pos_delta={:.4f} max_pos_delta={:.4f} snapped_changes={}",
                                   iter,
                                   static_cast<unsigned long long>(gid),
                                   static_cast<unsigned long long>(n),
                                   mean_pos_delta,
                                   max_pos_delta,
                                   static_cast<unsigned long long>(snapped_changes));
                }
            }

            bool consistent = reassignUsingSmoothed(smoothed_histories, frame_lookup, group_manager, ground_truth);
            if (consistent) {
                if (_logger) {
                    _logger->debug("iteration {} consistent; finishing", iter);
                }
                final_results = toResults(smoothed_histories);
                _pending_updates.flushToManager(group_manager);
                break;
            }

            // Prepare for next iteration: reset filters from smoothed start states
            resetFiltersFromSmoothed(smoothed_histories);
            prev_smoothed_histories = smoothed_histories;
            if (iter == max_iterations - 1) {
                if (_logger) {
                    _logger->debug("max iterations reached without consistency; returning last smoothed results");
                }
                final_results = toResults(smoothed_histories);
                _pending_updates.flushToManager(group_manager);
            }
        }

        return final_results;
    }

    /**
     * @brief Compatibility overload without max_iterations or progress.
     */
    template<typename Source>
        requires DataSource<Source, DataType>
    [[nodiscard]] SmoothedResults process(Source && data_source,
                                          EntityGroupManager & group_manager,
                                          GroundTruthMap const & ground_truth,
                                          TimeFrameIndex start_frame,
                                          TimeFrameIndex end_frame) {
        return process(std::forward<Source>(data_source),
                       group_manager,
                       ground_truth,
                       start_frame,
                       end_frame,
                       DEFAULT_MAX_ITERATIONS,
                       nullptr);
    }

    /**
     * @brief Compatibility overload matching the previous API (no max_iterations parameter).
     *
     * For now, runs a default limited number of iterations.
     *
     * @tparam Source Range of tuples (DataType, EntityId, TimeFrameIndex)
     * @param data_source Zero-copy data source
     * @param group_manager Group manager to read existing groups and record assignments
     * @param ground_truth Ground truth at specific frames for certain groups (anchors)
     * @param start_frame Inclusive start frame
     * @param end_frame Inclusive end frame
     * @param progress Optional progress callback
     * @pre start_frame <= end_frame
     * @post Returned results contain per-group smoothed states aligned with processed frames
     */
    template<typename Source>
        requires DataSource<Source, DataType>
    [[nodiscard]] SmoothedResults process(Source && data_source,
                                          EntityGroupManager & group_manager,
                                          GroundTruthMap & ground_truth,
                                          TimeFrameIndex & start_frame,
                                          TimeFrameIndex & end_frame,
                                          ProgressCallback & progress) {
        return process(std::forward<Source>(data_source),
                       group_manager,
                       static_cast<GroundTruthMap const &>(ground_truth),
                       static_cast<TimeFrameIndex>(start_frame),
                       static_cast<TimeFrameIndex>(end_frame),
                       DEFAULT_MAX_ITERATIONS,
                       progress);
    }

    /**
     * @brief Compatibility getter for identity confidence.
     * @param group_id Group identifier
     * @return Confidence in [0,1]; fixed 1.0 in this implementation.
     */
    [[nodiscard]] double getIdentityConfidence(GroupId /*group_id*/) const { return 1.0; }

    /**
     * @brief Compatibility getter for measurement noise scale.
     * @param group_id Group identifier
     * @return Fixed scale 1.0 (no dynamic inflation).
     */
    [[nodiscard]] double getMeasurementNoiseScale(GroupId /*group_id*/) const { return 1.0; }

    /**
     * @brief Compatibility getter for minimum confidence since last anchor.
     * @param group_id Group identifier
     * @return Fixed 1.0 in this implementation.
     */
    [[nodiscard]] double getMinConfidenceSinceAnchor(GroupId /*group_id*/) const { return 1.0; }

    void enableDebugLogging(std::string const & file_path) {
        _logger = std::make_shared<spdlog::logger>("Tracker", std::make_shared<spdlog::sinks::basic_file_sink_mt>(file_path));
        _logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        _logger->set_level(spdlog::level::debug);
        _logger->flush_on(spdlog::level::debug);
        spdlog::flush_every(std::chrono::seconds(1));
    }

private:
    static constexpr int DEFAULT_MAX_ITERATIONS = 3;
    struct GroupTrack {
        std::unique_ptr<IFilter> _filter;
        bool _active = false;
    };

    using FrameBucket = std::vector<std::tuple<DataType const *, EntityId, TimeFrameIndex>>;

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

    [[nodiscard]] std::map<GroupId, IntervalHistory>
    runForwardPass(std::map<TimeFrameIndex, FrameBucket> const & frame_lookup,
                   EntityGroupManager & group_manager,
                   GroundTruthMap const & ground_truth,
                   TimeFrameIndex start_frame,
                   TimeFrameIndex end_frame,
                   ProgressCallback const & progress,
                   std::optional<std::map<GroupId, IntervalHistory>> const & prev_smoothed_histories = std::nullopt) {
        std::map<GroupId, IntervalHistory> histories;

        // Activate tracks at first anchor with their initial state
        for (auto group_id: group_manager.getAllGroupIds()) {
            _tracks[group_id]->_active = false;
        }

        // Snapshot initially grouped entities for membership guards
        std::unordered_set<EntityId> initially_grouped_entities;
        for (auto gid: group_manager.getAllGroupIds()) {
            auto members = group_manager.getEntitiesInGroup(gid);
            initially_grouped_entities.insert(members.begin(), members.end());
        }

        int64_t processed = 0;
        int64_t const total = (end_frame - start_frame + TimeFrameIndex(1)).getValue();

        for (TimeFrameIndex f = start_frame; f <= end_frame; ++f) {
            auto const & bucket = getOrEmpty(frame_lookup, f);
            auto gt_it = ground_truth.find(f);

            // Build O(1) entity lookup for this frame
            std::unordered_map<EntityId, size_t> entity_to_index;
            entity_to_index.reserve(bucket.size());
            for (size_t i = 0; i < bucket.size(); ++i) {
                entity_to_index[std::get<1>(bucket[i])] = i;
            }

            // Per-frame guards
            std::unordered_set<GroupId> updated_groups_this_frame;
            std::unordered_set<EntityId> assigned_entities_this_frame;

            // Handle anchors: initialize or correct with measurement at low noise (but fixed R in filter)
            if (gt_it != ground_truth.end()) {
                for (auto const & [group_id, entity_id]: gt_it->second) {
                    auto & track = _tracks[group_id];
                    if (!track) continue;
                    DataType const * obs = nullptr;
                    auto it_idx = entity_to_index.find(entity_id);
                    if (it_idx != entity_to_index.end()) {
                        obs = std::get<0>(bucket[it_idx->second]);
                    }
                    if (!obs) continue;

                    std::optional<FilterState> anchor_predicted;
                    if (!track->_active) {
                        // First anchor: initialize filter from observation
                        auto init_state = _feature_extractor->getInitialState(*obs);
                        track->_filter->initialize(init_state);
                        track->_active = true;
                        // For forward prediction record, use the initialized state as the prediction at this anchor
                        anchor_predicted = init_state;
                        if (_logger) {
                            _logger->debug("frame {} anchor init: group={} entity={}", 
                                f.getValue(), 
                                static_cast<unsigned long long>(group_id), 
                                static_cast<unsigned long long>(entity_id.id));
                        }
                    } else {
                        // Predict before fusing the anchor; this is what we will validate against the anchor
                        auto pred = track->_filter->predict();
                        anchor_predicted = pred;
                        
                        // CRITICAL: Check if this prediction would actually assign to the ground truth entity
                        // This must be done BEFORE we fuse the anchor measurement
                        std::vector<Observation> anchor_obs;
                        std::map<EntityId, FeatureCache> anchor_cache;
                        for (auto const & item: bucket) {
                            auto const eid = std::get<1>(item);
                            auto const * data = std::get<0>(item);
                            anchor_obs.push_back({eid});
                            anchor_cache[eid] = _feature_extractor->getAllFeatures(*data);
                        }
                        
                        bool has_mismatch = false;
                        Assignment anchor_asn = _assigner->solve({{group_id, pred}}, anchor_obs, anchor_cache);
                        if (!anchor_asn.observation_to_prediction.empty()) {
                            EntityId predicted_pick = anchor_obs[anchor_asn.observation_to_prediction.begin()->first].entity_id;
                            if (predicted_pick != entity_id) {
                                has_mismatch = true;
                                if (_logger) {
                                    _logger->warn("frame {} anchor MISMATCH: group={} prediction would pick {} but ground truth is {}",
                                                  f.getValue(),
                                                  static_cast<unsigned long long>(group_id),
                                                  static_cast<unsigned long long>(predicted_pick.id),
                                                  static_cast<unsigned long long>(entity_id.id));
                                }
                                // Store this inconsistency to trigger re-iteration
                                _anchor_mismatches[f].insert(group_id);
                            }
                        }
                        
                        // If there's a mismatch, re-initialize the filter at the anchor
                        // This ensures the anchor strongly influences the smoothing backward
                        if (has_mismatch) {
                            // Re-initialize: trust the measurement completely with fresh uncertainty
                            auto init_state = _feature_extractor->getInitialState(*obs);
                            track->_filter->initialize(init_state);
                            if (_logger) {
                                _logger->debug("frame {} anchor REINITIALIZED due to mismatch: group={} entity={}", 
                                              f.getValue(), 
                                              static_cast<unsigned long long>(group_id), 
                                              static_cast<unsigned long long>(entity_id.id));
                            }
                        } else {
                            // Normal update when prediction matches ground truth
                            Measurement m = {_feature_extractor->getFilterFeatures(*obs)};
                            track->_filter->update(pred, m);
                        }
                        if (_logger) {
                            _logger->debug("frame {} anchor update: group={} entity={}", 
                                f.getValue(), 
                                static_cast<unsigned long long>(group_id), 
                                static_cast<unsigned long long>(entity_id.id));
                        }
                    }

                    auto & h = histories[group_id];
                    h.frames.push_back(f);
                    h.forward_predictions.push_back(anchor_predicted.value_or(track->_filter->getState()));
                    h.forward_states.push_back(track->_filter->getState());
                    h.assigned_entities.push_back(entity_id);

                    // Batch anchor membership update
                    _pending_updates.addPending(group_id, entity_id, f);
                    updated_groups_this_frame.insert(group_id);
                    assigned_entities_this_frame.insert(entity_id);
                }
            }

            // Build predictions for active tracks for assignment
            // If we have smoothed histories from previous iteration, use those for prediction
            // Otherwise use current filter predictions
            std::vector<Prediction> preds;
            std::map<GroupId, FilterState> last_pred_by_group;
            std::unordered_map<GroupId, size_t> pred_index_by_group;
            for (auto & [group_id, track]: _tracks) {
                if (!track || !track->_active) continue;
                
                FilterState pred;
                bool used_smoothed = false;
                
                // Try to get prediction from previous iteration's smoothed trajectory
                // Strategy: Predict from the PREVIOUS frame's smoothed state.
                // This uses the refined trajectory to make a better prediction into the current frame.
                if (prev_smoothed_histories.has_value()) {
                    auto it_hist = prev_smoothed_histories->find(group_id);
                    if (it_hist != prev_smoothed_histories->end()) {
                        auto const & hist = it_hist->second;
                        
                        // Find the index for the current frame `f` in the history
                        auto current_frame_it = std::find(hist.frames.begin(), hist.frames.end(), f);
                        
                        // Check if the current frame is in the history and not the first one
                        if (current_frame_it != hist.frames.end() && current_frame_it != hist.frames.begin()) {
                            size_t current_idx = std::distance(hist.frames.begin(), current_frame_it);
                            TimeFrameIndex prev_frame_in_hist = hist.frames[current_idx - 1];

                            // Only use smoothed prediction if the gap is not too large (i.e., not across a blackout)
                            if ((f - prev_frame_in_hist).getValue() <= 2) {
                                // Get the index of the previous frame in the history
                                size_t prev_idx = current_idx - 1;
                                
                                // Predict from the previous smoothed state
                                auto const & prev_smoothed_state = hist.forward_states[prev_idx];
                                auto tmp_filter = _filter_prototype->clone();
                                tmp_filter->initialize(prev_smoothed_state);
                                pred = tmp_filter->predict();
                                used_smoothed = true;

                                if (_logger) {
                                    _logger->debug("  group {} using SMOOTHED prediction from frame {}: pos=({:.2f}, {:.2f})",
                                                   static_cast<unsigned long long>(group_id),
                                                   hist.frames[prev_idx].getValue(),
                                                   pred.state_mean(0), pred.state_mean(1));
                                }
                            }
                        }
                    }
                }
                // Fall back to current filter prediction if no smoothed history available or gap is too large
                if (!used_smoothed) {
                    pred = track->_filter->predict();
                }
                
                if (updated_groups_this_frame.find(group_id) == updated_groups_this_frame.end()) {
                    pred_index_by_group[group_id] = preds.size();
                    preds.push_back({group_id, pred});
                    last_pred_by_group[group_id] = pred;
                    if (_logger && pred.state_mean.size() >= 2 && !used_smoothed) {
                        _logger->debug("  group {} predicts: pos=({:.2f}, {:.2f}) vel=({:.2f}, {:.2f})",
                                       static_cast<unsigned long long>(group_id),
                                       pred.state_mean(0), pred.state_mean(1),
                                       pred.state_mean.size() > 2 ? pred.state_mean(2) : 0.0,
                                       pred.state_mean.size() > 3 ? pred.state_mean(3) : 0.0);
                    }
                }
            }

            if (!preds.empty() && !bucket.empty()) {
                if (_logger) {
                    _logger->debug("frame {}: preds={} obs={}", f.getValue(), static_cast<unsigned long long>(preds.size()), static_cast<unsigned long long>(bucket.size()));
                    for (auto const & item: bucket) {
                        auto const eid = std::get<1>(item);
                        auto const * data = std::get<0>(item);

                        _logger->debug("  entity {} at", 
                                       static_cast<unsigned long long>(eid));
                    }
                }
                // First, apply planned/snapped assignments for this frame (fixed updates)
                std::vector<bool> pred_used(preds.size(), false);
                for (auto const & [gid, frames_map]: _planned_assignments) {
                    auto itpf = frames_map.find(f);
                    if (itpf == frames_map.end()) continue;
                    auto itpi = pred_index_by_group.find(gid);
                    if (itpi == pred_index_by_group.end()) continue;
                    size_t pidx = itpi->second;
                    if (pidx >= preds.size()) continue;
                    EntityId planned_entity = itpf->second;
                    // Fetch data for planned entity via entity_to_index
                    DataType const * obs_data = nullptr;
                    auto it_pl = entity_to_index.find(planned_entity);
                    if (it_pl != entity_to_index.end()) {
                        obs_data = std::get<0>(bucket[it_pl->second]);
                    }
                    if (!obs_data) continue;
                    Measurement m = {_feature_extractor->getFilterFeatures(*obs_data)};
                    auto & track = _tracks[preds[pidx].group_id];
                    if (!track || !track->_active) continue;
                    track->_filter->update(last_pred_by_group[preds[pidx].group_id], m);

                    auto & h = histories[preds[pidx].group_id];
                    h.frames.push_back(f);
                    h.forward_predictions.push_back(last_pred_by_group[preds[pidx].group_id]);
                    h.forward_states.push_back(track->_filter->getState());
                    h.assigned_entities.push_back(planned_entity);
                    _pending_updates.addPending(preds[pidx].group_id, planned_entity, f);
                    assigned_entities_this_frame.insert(planned_entity);
                    pred_used[pidx] = true;
                    if (_logger) {
                        _logger->debug("frame {} snapped: group={} <- entity={} (planned)",
                                       f.getValue(),
                                       static_cast<unsigned long long>(preds[pidx].group_id),
                                       static_cast<unsigned long long>(planned_entity.id));
                    }
                }

                // Build observations excluding already assigned/planned
                std::vector<Observation> obs;
                std::map<EntityId, FeatureCache> cache;
                obs.reserve(bucket.size());
                for (auto const & item: bucket) {
                    auto const eid = std::get<1>(item);
                    auto const * data = std::get<0>(item);
                    if (assigned_entities_this_frame.find(eid) != assigned_entities_this_frame.end()) continue;
                    if (initially_grouped_entities.find(eid) != initially_grouped_entities.end()) continue;
                    if (_pending_updates.contains(eid)) continue;
                    obs.push_back({eid});
                    cache[eid] = _feature_extractor->getAllFeatures(*data);
                }

                // Reduce predictions to those not already used by planned snapping
                std::vector<Prediction> preds_for_solver;
                std::vector<size_t> orig_pred_indices;
                preds_for_solver.reserve(preds.size());
                for (size_t i = 0; i < preds.size(); ++i) {
                    if (!pred_used[i]) {
                        preds_for_solver.push_back(preds[i]);
                        orig_pred_indices.push_back(i);
                    }
                }

                Assignment asn = _assigner->solve(preds_for_solver, obs, cache);
                for (auto const & [obs_idx, pred_idx]: asn.observation_to_prediction) {
                    auto const & chosen_obs = obs[static_cast<size_t>(obs_idx)];
                    size_t original_index = orig_pred_indices[static_cast<size_t>(pred_idx)];
                    auto const & chosen_pred = preds[original_index];

                    auto & track = _tracks[chosen_pred.group_id];
                    if (!track || !track->_active) continue;

                    DataType const * obs_data = nullptr;
                    auto it_obs = entity_to_index.find(chosen_obs.entity_id);
                    if (it_obs != entity_to_index.end()) {
                        obs_data = std::get<0>(bucket[it_obs->second]);
                    }
                    if (!obs_data) continue;

                    Measurement m = {_feature_extractor->getFilterFeatures(*obs_data)};
                    track->_filter->update(last_pred_by_group[chosen_pred.group_id], m);

                    auto & h = histories[chosen_pred.group_id];
                    h.frames.push_back(f);
                    h.forward_predictions.push_back(last_pred_by_group[chosen_pred.group_id]);
                    h.forward_states.push_back(track->_filter->getState());
                    h.assigned_entities.push_back(chosen_obs.entity_id);

                    // Batch assignment update
                    _pending_updates.addPending(chosen_pred.group_id, chosen_obs.entity_id, f);
                    assigned_entities_this_frame.insert(chosen_obs.entity_id);

                    if (_logger) {
                        _logger->debug("frame {} assign: group={} <- entity={} (obs_idx={} pred_idx={})",
                                       f.getValue(),
                                       static_cast<unsigned long long>(chosen_pred.group_id),
                                       static_cast<unsigned long long>(chosen_obs.entity_id.id),
                                       static_cast<unsigned long long>(obs_idx),
                                       static_cast<unsigned long long>(pred_idx));
                    }
                }
            }

            // progress
            if (progress) {
                processed++;
                int pct = static_cast<int>((processed * 100) / std::max<int64_t>(total, 1));
                progress(pct);
            }
        }

        return histories;
    }

    [[nodiscard]] std::map<GroupId, IntervalHistory>
    runRtsSmoothing(std::map<GroupId, IntervalHistory> const & forward_histories) const {
        std::map<GroupId, IntervalHistory> smoothed = forward_histories;
        for (auto & [group_id, hist]: smoothed) {
            if (hist.forward_states.size() <= 1) continue;
            // Use prototype filter to access smoother implementation over the forward states
            auto tmp = _filter_prototype->clone();
            auto smooth_states = tmp->smooth(hist.forward_states);
            hist.forward_states = smooth_states;
            if (_logger) {
                _logger->debug("smoothing: group={} states={}", static_cast<unsigned long long>(group_id), static_cast<unsigned long long>(hist.forward_states.size()));
            }
        }
        return smoothed;
    }

    [[nodiscard]] bool
    reassignUsingSmoothed(std::map<GroupId, IntervalHistory> const & smoothed_histories,
                          std::map<TimeFrameIndex, FrameBucket> const & frame_lookup,
                          EntityGroupManager & group_manager,
                          GroundTruthMap const & ground_truth) {
        bool all_consistent = true;  // Require BOTH anchor and frame consistency

        // First check: were there any anchor mismatches detected during forward pass?
        if (!_anchor_mismatches.empty()) {
            all_consistent = false;
            if (_logger) {
                for (auto const & [frame, groups]: _anchor_mismatches) {
                    for (auto gid: groups) {
                        _logger->debug("anchor mismatch detected at frame {} for group {} - triggering re-iteration",
                                       frame.getValue(),
                                       static_cast<unsigned long long>(gid));
                    }
                }
            }
        }

        // Strategy: For each frame, predict from the PREVIOUS smoothed state (not the current one!)
        // Then check if that prediction would have assigned to the same entity as the forward pass.
        // This is critical because the smoothed state AT a frame has already incorporated the 
        // measurement from that frame, so it's biased. We need the prediction INTO the frame.
        for (auto const & [group_id, hist]: smoothed_histories) {
            for (size_t i = 1; i < hist.frames.size(); ++i) { // Start at 1, need previous frame
                TimeFrameIndex frame = hist.frames[i];
                auto const & bucket = getOrEmpty(frame_lookup, frame);
                if (bucket.empty()) continue;

                // Skip anchors - they're always correct by definition
                auto gt_it = ground_truth.find(frame);
                if (gt_it != ground_truth.end() && gt_it->second.count(group_id) > 0) {
                    continue;
                }

                std::optional<EntityId> const & forward_assigned = hist.assigned_entities[i];
                if (!forward_assigned.has_value()) continue;

                // Check the gap between this frame and previous frame
                TimeFrameIndex prev_frame = hist.frames[i - 1];
                int gap = (frame - prev_frame).getValue();
                
                // Skip consistency check if gap is too large (e.g., after blackout)
                // Can't reliably predict across large gaps for consistency checking
                if (gap > 2) {
                    if (_logger) {
                        _logger->debug("frame {} consistency check skipped: gap from frame {} is {} (too large)",
                                       frame.getValue(),
                                       prev_frame.getValue(),
                                       gap);
                    }
                    continue;
                }

                // Get previous smoothed state and predict forward to current frame
                auto const & prev_smoothed = hist.forward_states[i - 1];
                auto tmp_filter = _filter_prototype->clone();
                tmp_filter->initialize(prev_smoothed);
                auto predicted_state = tmp_filter->predict();

                // Build observations for this frame
                std::vector<Observation> obs;
                std::map<EntityId, FeatureCache> cache;
                for (auto const & item: bucket) {
                    auto const eid = std::get<1>(item);
                    auto const * data = std::get<0>(item);
                    obs.push_back({eid});
                    cache[eid] = _feature_extractor->getAllFeatures(*data);
                }

                // What would the smoothed prediction assign to?
                Assignment asn = _assigner->solve({{group_id, predicted_state}}, obs, cache);
                if (asn.observation_to_prediction.empty()) continue;
                
                EntityId smoothed_picked = obs[asn.observation_to_prediction.begin()->first].entity_id;
                
                if (smoothed_picked != *forward_assigned) {
                    all_consistent = false;
                    if (_logger) {
                        _logger->debug("frame {} inconsistency: group={} forward-assigned {} but smoothed-prediction picks {}",
                                       frame.getValue(),
                                       static_cast<unsigned long long>(group_id),
                                       static_cast<unsigned long long>((*forward_assigned).id),
                                       static_cast<unsigned long long>(smoothed_picked.id));
                    }
                }
            }
        }

        // Additionally validate anchors using FORWARD predictions (pre-update)
        // AND check if smoothed state from previous frame would correctly predict anchor
        for (auto const & [frame, gt_for_frame]: ground_truth) {
            auto const & bucket = getOrEmpty(frame_lookup, frame);
            if (bucket.empty()) continue;

            for (auto const & [group_id, true_entity]: gt_for_frame) {
                auto it = smoothed_histories.find(group_id);
                if (it == smoothed_histories.end()) continue;

                auto const & hist = it->second;
                auto idx_it = std::find(hist.frames.begin(), hist.frames.end(), frame);
                if (idx_it == hist.frames.end()) continue;
                size_t idx = static_cast<size_t>(std::distance(hist.frames.begin(), idx_it));
                
                // Check 1: Use the forward prediction captured at anchor (before measurement update)
                auto const & predicted = hist.forward_predictions[idx];

                std::vector<Observation> obs;
                std::map<EntityId, FeatureCache> cache;
                for (auto const & item: bucket) {
                    auto const eid = std::get<1>(item);
                    auto const * data = std::get<0>(item);
                    obs.push_back({eid});
                    cache[eid] = _feature_extractor->getAllFeatures(*data);
                }
                
                Assignment asn = _assigner->solve({{group_id, predicted}}, obs, cache);
                if (asn.observation_to_prediction.empty()) continue;
                EntityId picked = obs[asn.observation_to_prediction.begin()->first].entity_id;
                
                if (picked != true_entity) {
                    all_consistent = false;
                    if (_logger) {
                        _logger->debug("anchor check frame {}: group={} predicted->{} != truth {}",
                                       frame.getValue(),
                                       static_cast<unsigned long long>(group_id),
                                       static_cast<unsigned long long>(picked.id),
                                       static_cast<unsigned long long>(true_entity.id));
                    }
                } else {
                    if (_logger) {
                        _logger->debug("anchor check frame {}: group={} predicted matches {}",
                                       frame.getValue(),
                                       static_cast<unsigned long long>(group_id),
                                       static_cast<unsigned long long>(true_entity.id));
                    }
                }
                
                // Check 2: Validate that SMOOTHED state from previous frame predicts correctly to anchor
                // This catches cases where forward pass picks wrong entity just before anchor
                if (idx > 0) {
                    auto const & prev_smoothed = hist.forward_states[idx - 1];
                    auto tmp_filter = _filter_prototype->clone();
                    tmp_filter->initialize(prev_smoothed);
                    auto smoothed_prediction = tmp_filter->predict();
                    
                    Assignment smoothed_asn = _assigner->solve({{group_id, smoothed_prediction}}, obs, cache);
                    if (!smoothed_asn.observation_to_prediction.empty()) {
                        EntityId smoothed_picked = obs[smoothed_asn.observation_to_prediction.begin()->first].entity_id;
                        if (smoothed_picked != true_entity) {
                            all_consistent = false;
                            if (_logger) {
                                _logger->debug("anchor check frame {}: group={} SMOOTHED-prediction from prev frame->{} != truth {}",
                                               frame.getValue(),
                                               static_cast<unsigned long long>(group_id),
                                               static_cast<unsigned long long>(smoothed_picked.id),
                                               static_cast<unsigned long long>(true_entity.id));
                            }
                        }
                    }
                }
            }
        }

        (void) group_manager;
        return all_consistent;
    }

    void resetFiltersFromSmoothed(std::map<GroupId, IntervalHistory> const & smoothed_histories) {
        for (auto const & [group_id, hist]: smoothed_histories) {
            if (hist.forward_states.empty()) continue;
            auto it = _tracks.find(group_id);
            if (it == _tracks.end() || !it->second) continue;
            it->second->_filter->initialize(hist.forward_states.front());
            it->second->_active = true;
        }
    }

    static DataType const * findEntity(FrameBucket const & bucket, EntityId id) {
        for (auto const & item: bucket) {
            if (std::get<1>(item) == id) return std::get<0>(item);
        }
        return nullptr;
    }

    static FrameBucket const & getOrEmpty(std::map<TimeFrameIndex, FrameBucket> const & frames, TimeFrameIndex f) {
        static FrameBucket empty;
        auto it = frames.find(f);
        if (it == frames.end()) return empty;
        return it->second;
    }

    [[nodiscard]] static SmoothedResults toResults(std::map<GroupId, IntervalHistory> const & histories) {
        SmoothedResults out;
        for (auto const & [gid, hist]: histories) {
            out[gid] = hist.forward_states;
        }
        return out;
    }

    void buildSnappedAssignments(std::map<GroupId, IntervalHistory> const & smoothed_histories,
                                 std::map<TimeFrameIndex, FrameBucket> const & frame_lookup,
                                 GroundTruthMap const & ground_truth) {
        for (auto const & [gid, hist]: smoothed_histories) {
            for (size_t k = 0; k < hist.frames.size(); ++k) {
                TimeFrameIndex f = hist.frames[k];
                // Respect anchors
                auto gt_it = ground_truth.find(f);
                if (gt_it != ground_truth.end()) {
                    auto const & gt_for_frame = gt_it->second;
                    auto ggit = gt_for_frame.find(gid);
                    if (ggit != gt_for_frame.end()) {
                        _planned_assignments[gid][f] = ggit->second;
                        continue;
                    }
                }

                auto const & bucket = getOrEmpty(frame_lookup, f);
                if (bucket.empty()) continue;
                auto const & state = hist.forward_states[k];

                // Build obs/cache
                std::vector<Observation> obs;
                std::map<EntityId, FeatureCache> cache;
                for (auto const & item: bucket) {
                    auto eid = std::get<1>(item);
                    auto const * data = std::get<0>(item);
                    obs.push_back({eid});
                    cache[eid] = _feature_extractor->getAllFeatures(*data);
                }
                Assignment asn = _assigner->solve({{gid, state}}, obs, cache);
                if (!asn.observation_to_prediction.empty()) {
                    EntityId picked = obs[asn.observation_to_prediction.begin()->first].entity_id;
                    _planned_assignments[gid][f] = picked;
                }
            }
        }
    }

private:
    std::unique_ptr<IFilter> _filter_prototype;
    std::unique_ptr<IFeatureExtractor<DataType>> _feature_extractor;
    std::unique_ptr<IAssigner> _assigner;
    std::unordered_map<GroupId, std::unique_ptr<GroupTrack>> _tracks;
    std::shared_ptr<spdlog::logger> _logger;

    struct PendingGroupUpdates {
        std::unordered_map<GroupId, std::vector<std::pair<TimeFrameIndex, EntityId>>> pending_additions;
        std::unordered_set<EntityId> entities_added_this_pass;

        void addPending(GroupId group_id, EntityId entity_id, TimeFrameIndex frame) {
            pending_additions[group_id].emplace_back(frame, entity_id);
            entities_added_this_pass.insert(entity_id);
        }

        bool contains(EntityId entity_id) const {
            return entities_added_this_pass.find(entity_id) != entities_added_this_pass.end();
        }

        void clear() {
            pending_additions.clear();
            entities_added_this_pass.clear();
        }

        void flushToManager(EntityGroupManager & manager) {
            for (auto const & [gid, entries]: pending_additions) {
                for (auto const & entry: entries) {
                    manager.addEntityToGroup(gid, entry.second);
                }
            }
            clear();
        }
    };

    PendingGroupUpdates _pending_updates;

    // Planned snapped assignments per group per frame for the next iteration
    std::unordered_map<GroupId, std::map<TimeFrameIndex, EntityId>> _planned_assignments;

    // Track which groups had mismatches at which anchor frames (detected during forward pass)
    std::map<TimeFrameIndex, std::unordered_set<GroupId>> _anchor_mismatches;
};

}// namespace StateEstimation

#endif// TRACKER_NEW_HPP
