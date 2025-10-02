#ifndef TRACKER_HPP
#define TRACKER_HPP

#include "Assignment/IAssigner.hpp"
#include "DataSource.hpp"
#include "Features/IFeatureExtractor.hpp"
#include "Filter/IFilter.hpp"
#include "IdentityConfidence.hpp"

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace StateEstimation {

// The return type: a map from each GroupId to its series of smoothed states.
using SmoothedResults = std::map<GroupId, std::vector<FilterState>>;

// Progress callback: takes percentage (0-100) and current frame
using ProgressCallback = std::function<void(int)>;

// Forward declaration for the state structure
template<typename DataType>
struct TrackedGroupState;

/**
 * @brief Helper structure for batching group assignment updates.
 * 
 * Accumulates entity-to-group assignments during tracking and flushes them
 * to the EntityGroupManager at strategic points (anchor frames). This provides
 * significant performance benefits by:
 * - Avoiding O(G × E_g × log E) cost of rebuilding group membership set every frame
 * - Providing O(1) membership checks via hash set
 * - Batching updates for better cache locality
 */
struct PendingGroupUpdates {
    // Frame-aware pending additions per group
    std::unordered_map<GroupId, std::vector<std::pair<TimeFrameIndex, EntityId>>> pending_additions;

    // Fast O(1) lookup for entities assigned during this pass
    std::unordered_set<EntityId> entities_added_this_pass;

    void addPending(GroupId group_id, EntityId entity_id, TimeFrameIndex frame) {
        pending_additions[group_id].emplace_back(frame, entity_id);
        entities_added_this_pass.insert(entity_id);
    }

    // Replace the entity assigned for a given group and frame, if present
    void replaceForFrame(GroupId group_id, TimeFrameIndex frame, EntityId new_entity_id) {
        auto it = pending_additions.find(group_id);
        if (it == pending_additions.end()) return;
        for (auto & entry: it->second) {
            if (entry.first == frame) {
                entry.second = new_entity_id;
            }
        }
        entities_added_this_pass.insert(new_entity_id);
    }

    void flushToManager(EntityGroupManager & manager) {
        for (auto const & [group_id, entries]: pending_additions) {
            for (auto const & [/*frame*/ _, entity_id]: entries) {
                manager.addEntityToGroup(group_id, entity_id);
            }
        }
        pending_additions.clear();
        entities_added_this_pass.clear();
    }

    bool contains(EntityId entity_id) const {
        return entities_added_this_pass.find(entity_id) != entities_added_this_pass.end();
    }

    std::unordered_set<EntityId> const & getAddedEntities() const {
        return entities_added_this_pass;
    }
};

/**
 * @brief The central orchestrator for the tracking process.
 *
 * This class manages the lifecycle of tracked objects (groups) and coordinates
 * the filter, feature extraction, and assignment components to process data
 * across multiple time frames. It is templated on the raw data type it operates on.
 *
 * @tparam DataType The raw data type (e.g., Line2D, Point2D).
 */
template<typename DataType>
class Tracker {
public:
    using GroundTruthMap = std::map<TimeFrameIndex, std::map<GroupId, EntityId>>;

    /**
     * @brief Constructs a Tracker.
     *
     * @param filter_prototype A prototype of the filter to be used for each track. It will be cloned.
     * @param feature_extractor A unique_ptr to the feature extractor strategy.
     * @param assigner A unique_ptr to the assignment strategy. Can be nullptr for smoothing-only tasks.
     */
    Tracker(std::unique_ptr<IFilter> filter_prototype,
            std::unique_ptr<IFeatureExtractor<DataType>> feature_extractor,
            std::unique_ptr<IAssigner> assigner = nullptr)
        : filter_prototype_(std::move(filter_prototype)),
          feature_extractor_(std::move(feature_extractor)),
          assigner_(std::move(assigner)) {}

    /**
     * @brief Enable detailed debug logging to a file.
     * @pre File path is writable.
     * @post Subsequent calls to process will emit per-frame diagnostics.
     */
    void enableDebugLogging(std::string const & file_path) {
        try {
            logger_ = spdlog::basic_logger_mt("TrackerLogger", file_path);
            logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
            logger_->set_level(spdlog::level::debug);
        } catch (spdlog::spdlog_ex const &) {
            logger_ = spdlog::get("TrackerLogger");
            if (logger_) {
                logger_->set_level(spdlog::level::debug);
            }
        }
    }

    /**
     * @brief Main processing entry point. Runs the tracking algorithm using a zero-copy data source.
     *
     * @tparam Source A range type satisfying the DataSource concept
     * @param data_source A range providing tuples of (DataType, EntityId, TimeFrameIndex)
     * @param group_manager The EntityGroupManager containing group assignments. Will be modified for new assignments.
     * @param ground_truth A map indicating ground-truth labels for specific groups at specific frames.
     * @param start_frame The first frame to process.
     * @param end_frame The last frame to process.
     * @param progress_callback Optional callback for progress reporting (percentage 0-100).
     * @return A map from GroupId to a vector of smoothed filter states.
     */
    template<typename Source>
        requires DataSource<Source, DataType>
    SmoothedResults process(Source && data_source,
                            EntityGroupManager & group_manager,
                            GroundTruthMap const & ground_truth,
                            TimeFrameIndex start_frame,
                            TimeFrameIndex end_frame,
                            ProgressCallback progress_callback = nullptr) {

        // Build frame-indexed lookup for efficient access
        std::map<TimeFrameIndex, std::vector<std::tuple<DataType const *, EntityId, TimeFrameIndex>>> frame_data_lookup;

        for (auto const & item: data_source) {
            TimeFrameIndex time = getTimeFrameIndex(item);
            if (time >= start_frame && time <= end_frame) {
                frame_data_lookup[time].emplace_back(&getData(item), getEntityId(item), time);
            }
        }

        // Initialize tracks from EntityGroupManager
        for (auto group_id: group_manager.getAllGroupIds()) {
            if (active_tracks_.find(group_id) == active_tracks_.end()) {
                active_tracks_[group_id] = TrackedGroupState<DataType>{
                        .group_id = group_id,
                        .filter = filter_prototype_->clone(),
                        .is_active = false,
                        .frames_since_last_seen = 0,
                        .confidence = 1.0,
                        .identity_confidence = IdentityConfidence{},
                        .anchor_frames = {},
                        .forward_pass_history = {},
                        .forward_prediction_history = {},
                        .processed_frames_history = {},
                        .identity_confidence_history = {},
                        .assigned_entity_history = {}};
            }
        }

        // OPTIMIZATION 1: Build initial grouped entities set ONCE
        // This avoids O(G × E_g × log E) rebuild every frame
        std::unordered_set<EntityId> initially_grouped_entities;
        for (auto group_id: group_manager.getAllGroupIds()) {
            auto entities = group_manager.getEntitiesInGroup(group_id);
            initially_grouped_entities.insert(entities.begin(), entities.end());
        }

        // OPTIMIZATION 1: Deferred group updates for batch processing
        PendingGroupUpdates pending_updates;

        SmoothedResults all_smoothed_results;

        TimeFrameIndex const total_frames = end_frame - start_frame + TimeFrameIndex(1);
        int64_t frames_processed = 0;

        for (TimeFrameIndex current_frame = start_frame; current_frame <= end_frame; ++current_frame) {
            auto frame_data_it = frame_data_lookup.find(current_frame);
            auto const & all_frame_data = (frame_data_it != frame_data_lookup.end())
                                                  ? frame_data_it->second
                                                  : std::vector<std::tuple<DataType const *, EntityId, TimeFrameIndex>>{};

            // OPTIMIZATION 2: Build per-frame entity index for O(1) entity lookup
            // Eliminates O(E) linear searches repeated 3+ times per frame
            std::unordered_map<EntityId, size_t> entity_to_index;
            for (size_t i = 0; i < all_frame_data.size(); ++i) {
                EntityId eid = std::get<1>(all_frame_data[i]);
                entity_to_index[eid] = i;
            }

            // Report progress
            if (progress_callback) {
                ++frames_processed;
                int const percentage = static_cast<int>((frames_processed * 100) / total_frames.getValue());
                progress_callback(percentage);
            }

            if (logger_) {
                logger_->debug("frame={} entities={} active_groups={}",
                               current_frame.getValue(),
                               all_frame_data.size(),
                               active_tracks_.size());
            }

            // --- Predictions ---
            std::map<GroupId, FilterState> predictions;
            for (auto & [group_id, track]: active_tracks_) {
                if (track.is_active) {
                    predictions[group_id] = track.filter->predict();
                    track.frames_since_last_seen++;
                }
            }

            auto gt_frame_it = ground_truth.find(current_frame);

            std::set<GroupId> updated_groups_this_frame;
            std::set<EntityId> assigned_entities_this_frame;
            std::unordered_map<GroupId, std::optional<EntityId>> group_assigned_entity_in_frame;

            // --- Ground Truth Updates & Activation ---
            processGroundTruthUpdates(current_frame,
                                      ground_truth,
                                      all_frame_data,
                                      entity_to_index,
                                      predictions,
                                      updated_groups_this_frame,
                                      assigned_entities_this_frame);

            // --- Assignment for Ungrouped Data ---
            if (assigner_) {
                std::vector<Observation> observations;
                std::map<EntityId, FeatureCache> feature_cache;

                // OPTIMIZATION 1: O(1) membership checks using cached sets
                // Avoids O(G × E_g × log E) rebuild every frame
                for (auto const & [data_ptr, entity_id, time]: all_frame_data) {
                    if (assigned_entities_this_frame.find(entity_id) == assigned_entities_this_frame.end() &&
                        initially_grouped_entities.find(entity_id) == initially_grouped_entities.end() &&
                        !pending_updates.contains(entity_id)) {
                        observations.push_back({entity_id});
                        feature_cache[entity_id] = feature_extractor_->getAllFeatures(*data_ptr);
                    }
                }

                std::vector<Prediction> prediction_list;
                for (auto const & [group_id, pred_state]: predictions) {
                    if (active_tracks_.at(group_id).is_active &&
                        updated_groups_this_frame.find(group_id) == updated_groups_this_frame.end()) {
                        // Do not allow assignment to groups already updated by ground truth this frame
                        prediction_list.push_back({group_id, pred_state});
                    }
                }

                if (!observations.empty() && !prediction_list.empty()) {
                    Assignment assignment = assigner_->solve(prediction_list, observations, feature_cache);

                    for (auto const & [obs_idx, pred_idx]: assignment.observation_to_prediction) {
                        auto const & obs = observations[static_cast<size_t>(obs_idx)];
                        auto const & pred = prediction_list[static_cast<size_t>(pred_idx)];

                        auto & track = active_tracks_.at(pred.group_id);

                        // OPTIMIZATION 2: O(1) entity lookup instead of O(E) linear search
                        auto entity_it = entity_to_index.find(obs.entity_id);
                        if (entity_it == entity_to_index.end()) continue;

                        DataType const * obs_data = std::get<0>(all_frame_data[entity_it->second]);

                        // Update identity confidence based on assignment cost
                        auto cost_it = assignment.assignment_costs.find(obs_idx);
                        if (cost_it != assignment.assignment_costs.end()) {
                            track.identity_confidence.updateOnAssignment(cost_it->second, assignment.cost_threshold);

                            // Allow slow recovery for excellent assignments
                            double excellent_threshold = assignment.cost_threshold * 0.1;
                            track.identity_confidence.allowSlowRecovery(cost_it->second, excellent_threshold);
                            if (logger_) {
                                logger_->debug("assign f={} g={} obs={} cost={:.3f} conf={:.3f}",
                                               current_frame.getValue(),
                                               pred.group_id,
                                               obs.entity_id,
                                               cost_it->second,
                                               track.identity_confidence.getConfidence());
                            }
                        }

                        // Scale measurement noise based on identity confidence
                        double noise_scale = track.identity_confidence.getMeasurementNoiseScale();
                        Measurement measurement = {feature_extractor_->getFilterFeatures(*obs_data)};

                        // Apply noise scaling to the measurement
                        track.filter->update(pred.filter_state, measurement, noise_scale);

                        if (logger_) {
                            double cov_tr = track.filter->getState().state_covariance.trace();
                            logger_->debug("update f={} g={} obs={} noise_scale={:.3f} cov_tr={:.3f}",
                                           current_frame.getValue(),
                                           pred.group_id,
                                           obs.entity_id,
                                           noise_scale, cov_tr);
                        }

                        // OPTIMIZATION 1: Defer update to batch flush at anchor frames (track frame)
                        pending_updates.addPending(pred.group_id, obs.entity_id, current_frame);

                        updated_groups_this_frame.insert(pred.group_id);
                        assigned_entities_this_frame.insert(obs.entity_id);
                        group_assigned_entity_in_frame[pred.group_id] = obs.entity_id;
                        track.frames_since_last_seen = 0;
                    }
                }
            }// end of assignment for ungrouped data

            // --- Finalize Frame State, Log History, and Handle Smoothing ---
            bool any_smoothing_this_frame = false;
            for (auto & [group_id, track]: active_tracks_) {
                if (!track.is_active) continue;

                // If a track was NOT updated this frame, its state is the predicted state. Commit it.
                if (updated_groups_this_frame.find(group_id) == updated_groups_this_frame.end()) {
                    track.filter->initialize(predictions.at(group_id));
                }

                // Record histories aligned by frame
                track.forward_pass_history.push_back(track.filter->getState());
                auto pred_it_for_hist = predictions.find(group_id);
                if (pred_it_for_hist != predictions.end()) {
                    track.forward_prediction_history.push_back(pred_it_for_hist->second);
                } else {
                    // At activation frames we may not have a prediction; use current state as placeholder
                    track.forward_prediction_history.push_back(track.filter->getState());
                }
                track.processed_frames_history.push_back(current_frame);
                track.identity_confidence_history.push_back(track.identity_confidence.getConfidence());
                if (auto it_assigned = group_assigned_entity_in_frame.find(group_id); it_assigned != group_assigned_entity_in_frame.end()) {
                    track.assigned_entity_history.push_back(it_assigned->second);
                } else {
                    track.assigned_entity_history.push_back(std::nullopt);
                }

                // Check for smoothing trigger on new anchor frames
                bool is_anchor = (gt_frame_it != ground_truth.end() && gt_frame_it->second.count(group_id));
                if (is_anchor) {
                    if (std::find(track.anchor_frames.begin(), track.anchor_frames.end(), current_frame) == track.anchor_frames.end()) {
                        track.anchor_frames.push_back(current_frame);
                    }

                    if (track.anchor_frames.size() >= 2) {
                        // 1) Smooth forward history between anchors
                        auto smoothed = track.filter->smooth(track.forward_pass_history);

                        // 1a) Assignment-aware covariance inflation with anchor proximity decay
                        // Reduce inflation near anchors so anchor certainty propagates backwards/forwards
                        if (smoothed.size() >= 2) {
                            size_t const Nseg = smoothed.size();
                            double const nminus = static_cast<double>(Nseg - 1);
                            double const half_len = nminus / 2.0;
                            for (size_t i = 0; i < Nseg; ++i) {
                                double const conf = track.identity_confidence_history[static_cast<size_t>(i)];
                                // Proximity weight: 0 at anchors, 1 at the midpoint
                                double const i_d = static_cast<double>(i);
                                double const d_to_edge = std::min(i_d, nminus - i_d);
                                double const proximity_weight = (half_len <= 0.0) ? 0.0 : std::clamp(d_to_edge / half_len, 0.0, 1.0);
                                // Map confidence to base inflation then attenuate by proximity to anchors
                                double const base_inflation = (conf <= 0.0) ? 1.0 : std::max(1.0, 1.0 + (1.0 - conf) * 2.0);
                                double const inflation = 1.0 + (base_inflation - 1.0) * proximity_weight;
                                smoothed[i].state_covariance *= inflation;
                                if (logger_) {
                                    double tr = smoothed[i].state_covariance.trace();
                                    logger_->debug("smooth f={} g={} i={} conf={:.3f} infl={:.3f} cov_tr={:.3f}",
                                                   track.processed_frames_history[i].getValue(),
                                                   group_id,
                                                   i,
                                                   conf,
                                                   inflation, tr);
                                }
                            }
                        }

                        // 2) Reverse-pass reconciliation via assigner using smoothed predictions
                        if (assigner_) {
                            for (size_t i = 0; i < smoothed.size(); ++i) {
                                TimeFrameIndex const frame = track.processed_frames_history[static_cast<size_t>(i)];

                                // Do not reconcile on anchor frames (respect labels)
                                auto gt_it_for_frame = ground_truth.find(frame);
                                if (gt_it_for_frame != ground_truth.end() && gt_it_for_frame->second.count(group_id)) {
                                    continue;
                                }

                                // Build observation set for frame
                                std::vector<Observation> observations;
                                std::map<EntityId, FeatureCache> feature_cache;
                                auto fd_it = frame_data_lookup.find(frame);
                                if (fd_it != frame_data_lookup.end()) {
                                    auto const & frame_items = fd_it->second;
                                    observations.reserve(frame_items.size());
                                    for (auto const & [data_ptr, entity_id, /*time*/ _t]: frame_items) {
                                        observations.push_back({entity_id});
                                        feature_cache[entity_id] = feature_extractor_->getAllFeatures(*data_ptr);
                                    }
                                }
                                if (observations.empty()) continue;

                                // Build prediction lists: smoothed and forward predicted
                                std::vector<Prediction> pred_smoothed = {{group_id, smoothed[i]}};
                                std::vector<Prediction> pred_forward = {{group_id, track.forward_prediction_history[static_cast<size_t>(i)]}};

                                auto assign_smoothed = assigner_->solve(pred_smoothed, observations, feature_cache);
                                auto assign_forward = assigner_->solve(pred_forward, observations, feature_cache);

                                // Extract chosen entities and costs, if any
                                auto pick_entity_and_cost = [](Assignment const & a) -> std::optional<std::pair<int, double>> {
                                    if (a.observation_to_prediction.empty()) return std::nullopt;
                                    auto const & kv = *a.observation_to_prediction.begin();
                                    int obs_idx = kv.first;
                                    auto itc = a.assignment_costs.find(obs_idx);
                                    double cost = (itc != a.assignment_costs.end()) ? itc->second : std::numeric_limits<double>::infinity();
                                    return std::make_optional(std::make_pair(obs_idx, cost));
                                };

                                auto sm_pick = pick_entity_and_cost(assign_smoothed);
                                auto fwd_pick = pick_entity_and_cost(assign_forward);
                                if (!sm_pick && !fwd_pick) continue;

                                // Choose by assignment cost; if similar, prefer lower predicted covariance (more certain)
                                int chosen_obs_idx = -1;
                                if (sm_pick && fwd_pick) {
                                    double const cost_sm = sm_pick->second;
                                    double const cost_fw = fwd_pick->second;
                                    double const epsilon_cost = 0.5;// tie-break threshold
                                    if (std::abs(cost_sm - cost_fw) <= epsilon_cost) {
                                        double const cov_sm = smoothed[i].state_covariance.trace();
                                        double const cov_fw = track.forward_prediction_history[static_cast<size_t>(i)].state_covariance.trace();
                                        chosen_obs_idx = (cov_sm <= cov_fw) ? sm_pick->first : fwd_pick->first;
                                    } else {
                                        chosen_obs_idx = (cost_sm <= cost_fw) ? sm_pick->first : fwd_pick->first;
                                    }
                                    if (logger_) {
                                        logger_->debug("reconcile f={} g={} cost_sm={:.3f} cost_fw={:.3f} cov_sm={:.3f} cov_fw={:.3f}",
                                                       frame.getValue(),
                                                       group_id,
                                                       cost_sm,
                                                       cost_fw,
                                                       smoothed[i].state_covariance.trace(),
                                                       track.forward_prediction_history[static_cast<size_t>(i)].state_covariance.trace());
                                    }
                                } else if (sm_pick) {
                                    chosen_obs_idx = sm_pick->first;
                                } else {
                                    chosen_obs_idx = fwd_pick->first;
                                }

                                // Resolve entity id from chosen_obs_idx
                                auto fd2_it = frame_data_lookup.find(frame);
                                if (fd2_it == frame_data_lookup.end()) continue;
                                auto const & frame_items2 = fd2_it->second;
                                if (chosen_obs_idx < 0 || static_cast<size_t>(chosen_obs_idx) >= frame_items2.size()) continue;
                                EntityId chosen_entity = std::get<1>(frame_items2[static_cast<size_t>(chosen_obs_idx)]);

                                // If differs from forward assignment for this frame, replace in pending updates
                                auto const & recorded = track.assigned_entity_history[static_cast<size_t>(i)];
                                if (!recorded.has_value() || recorded.value() != chosen_entity) {
                                    pending_updates.replaceForFrame(group_id, frame, chosen_entity);
                                    if (logger_) {
                                        logger_->debug("replace f={} g={} old={} new={}",
                                                       frame.getValue(),
                                                       group_id,
                                                       recorded.has_value() ? recorded.value() : -1, chosen_entity);
                                    }
                                }
                            }
                        }

                        if (all_smoothed_results[group_id].empty()) {
                            all_smoothed_results[group_id] = smoothed;
                        } else {
                            all_smoothed_results[group_id].insert(
                                    all_smoothed_results[group_id].end(),
                                    std::next(smoothed.begin()),
                                    smoothed.end());
                        }

                        // Collapse histories to keep only the last element for continuity into next interval
                        track.forward_pass_history.clear();
                        track.forward_pass_history.push_back(smoothed.back());
                        // Align auxiliary histories
                        TimeFrameIndex last_frame = track.processed_frames_history.back();
                        track.forward_prediction_history.clear();
                        track.forward_prediction_history.push_back(smoothed.back());
                        track.processed_frames_history.clear();
                        track.processed_frames_history.push_back(last_frame);
                        track.identity_confidence_history.clear();
                        track.identity_confidence_history.push_back(track.identity_confidence.getConfidence());
                        track.assigned_entity_history.clear();
                        track.assigned_entity_history.push_back(std::nullopt);
                        // Keep only the last anchor for the next interval
                        track.anchor_frames = {current_frame};

                        any_smoothing_this_frame = true;
                    }
                }// end of anchor frame backward pass
            }

            // OPTIMIZATION 1: Flush pending updates at anchor frames (smoothing boundaries)
            // This is the natural synchronization point between tracking intervals
            if (any_smoothing_this_frame) {
                pending_updates.flushToManager(group_manager);

                // Update the cached grouped entities set for the next interval
                auto const & newly_added = pending_updates.getAddedEntities();
                initially_grouped_entities.insert(newly_added.begin(), newly_added.end());
            }
        }

        // Final flush of any remaining pending updates
        pending_updates.flushToManager(group_manager);

        return all_smoothed_results;
    }

    /**
     * @brief Gets the current identity confidence for a specific group.
     * @param group_id The group to query
     * @return The current identity confidence [0.1, 1.0], or -1.0 if group not found
     */
    double getIdentityConfidence(GroupId group_id) const {
        auto it = active_tracks_.find(group_id);
        if (it == active_tracks_.end()) return -1.0;
        return it->second.identity_confidence.getConfidence();
    }

    /**
     * @brief Gets the measurement noise scale factor for a specific group.
     * @param group_id The group to query
     * @return The current noise scale factor, or -1.0 if group not found
     */
    double getMeasurementNoiseScale(GroupId group_id) const {
        auto it = active_tracks_.find(group_id);
        if (it == active_tracks_.end()) return -1.0;
        return it->second.identity_confidence.getMeasurementNoiseScale();
    }

    /**
     * @brief Gets the minimum confidence since last anchor for a specific group.
     * @param group_id The group to query
     * @return The minimum confidence since last anchor, or -1.0 if group not found
     */
    double getMinConfidenceSinceAnchor(GroupId group_id) const {
        auto it = active_tracks_.find(group_id);
        if (it == active_tracks_.end()) return -1.0;
        return it->second.identity_confidence.getMinConfidenceSinceAnchor();
    }

private:
    /**
     * @brief Processes ground truth updates and activates tracks for the current frame.
     * 
     * @param current_frame The current frame being processed
     * @param ground_truth The ground truth map for all frames
     * @param all_frame_data The data for the current frame
     * @param entity_to_index Fast lookup map from EntityId to data index
     * @param predictions The predicted states for all active tracks
     * @param updated_groups_this_frame Set of groups updated this frame (modified)
     * @param assigned_entities_this_frame Set of entities assigned this frame (modified)
     */
    void processGroundTruthUpdates(
            TimeFrameIndex current_frame,
            GroundTruthMap const & ground_truth,
            std::vector<std::tuple<DataType const *, EntityId, TimeFrameIndex>> const & all_frame_data,
            std::unordered_map<EntityId, size_t> const & entity_to_index,
            std::map<GroupId, FilterState> const & predictions,
            std::set<GroupId> & updated_groups_this_frame,
            std::set<EntityId> & assigned_entities_this_frame) {

        auto gt_frame_it = ground_truth.find(current_frame);
        if (gt_frame_it != ground_truth.end()) {
            for (auto const & [group_id, entity_id]: gt_frame_it->second) {
                auto track_it = active_tracks_.find(group_id);
                if (track_it == active_tracks_.end()) continue;
                auto & track = track_it->second;

                // OPTIMIZATION 2: O(1) entity lookup instead of O(E) linear search
                auto entity_it = entity_to_index.find(entity_id);
                if (entity_it == entity_to_index.end()) continue;

                DataType const * gt_item = std::get<0>(all_frame_data[entity_it->second]);

                if (!track.is_active) {
                    track.filter->initialize(feature_extractor_->getInitialState(*gt_item));
                    track.is_active = true;
                } else {
                    Measurement measurement = {feature_extractor_->getFilterFeatures(*gt_item)};
                    // Strengthen anchor certainty by reducing measurement noise at GT frames
                    double const anchor_noise_scale = 0.25;
                    track.filter->update(predictions.at(group_id), measurement, anchor_noise_scale);
                }

                // Reset identity confidence on ground truth updates
                track.identity_confidence.resetOnGroundTruth();

                track.frames_since_last_seen = 0;
                updated_groups_this_frame.insert(group_id);
                assigned_entities_this_frame.insert(entity_id);
            }
        }
    }

    std::unique_ptr<IFilter> filter_prototype_;
    std::unique_ptr<IFeatureExtractor<DataType>> feature_extractor_;
    std::unique_ptr<IAssigner> assigner_;
    std::unordered_map<GroupId, TrackedGroupState<DataType>> active_tracks_;
    std::shared_ptr<spdlog::logger> logger_;
};


/**
 * @brief Holds the state for a single tracked group.
 * @tparam DataType The raw data type (e.g., Line2D, Point2D).
 */
template<typename DataType>
struct TrackedGroupState {
    GroupId group_id;
    std::unique_ptr<IFilter> filter;
    bool is_active = false;
    int frames_since_last_seen = 0;
    double confidence = 1.0;

    // Identity confidence tracking for assignment uncertainty
    IdentityConfidence identity_confidence;

    // History for smoothing between anchors
    std::vector<TimeFrameIndex> anchor_frames = {};
    std::vector<FilterState> forward_pass_history = {};
    // Auxiliary histories aligned with forward_pass_history indices
    std::vector<FilterState> forward_prediction_history = {};
    std::vector<TimeFrameIndex> processed_frames_history = {};
    std::vector<double> identity_confidence_history = {};
    std::vector<std::optional<EntityId>> assigned_entity_history = {};
};

}// namespace StateEstimation


#endif// TRACKER_HPP
