#ifndef TRACKER_HPP
#define TRACKER_HPP

#include "Assignment/IAssigner.hpp"
#include "DataSource.hpp"
#include "Features/IFeatureExtractor.hpp"
#include "Filter/IFilter.hpp"
#include "IdentityConfidence.hpp"

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <chrono>
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
            // Use a rotating sink to avoid losing logs in long runs
            std::size_t const max_bytes = static_cast<std::size_t>(500) * 1024 * 1024;// 500 MB
            std::size_t const max_files = 3;
            auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file_path, max_bytes, max_files);
            auto existing = spdlog::get("TrackerLogger");
            if (existing) {
                logger_ = existing;
                logger_->sinks().clear();
                logger_->sinks().push_back(sink);
            } else {
                logger_ = std::make_shared<spdlog::logger>("TrackerLogger", sink);
                spdlog::register_logger(logger_);
            }
            logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
            logger_->set_level(spdlog::level::debug);
            logger_->flush_on(spdlog::level::debug);
            spdlog::flush_every(std::chrono::seconds(1));
        } catch (spdlog::spdlog_ex const &) {
            logger_ = spdlog::get("TrackerLogger");
            if (logger_) {
                logger_->set_level(spdlog::level::debug);
                logger_->flush_on(spdlog::level::debug);
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

            // **FIX 3: SYNCHRONIZE PREDICTION HISTORY**
            // For any group updated by GT, overwrite its prediction with the certain, GT-updated state.
            for (GroupId group_id: updated_groups_this_frame) {
                if (active_tracks_.count(group_id)) {
                    predictions[group_id] = active_tracks_.at(group_id).filter->getState();
                }
            }

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
                        /**************************************************************************
                         * RE-IMPLEMENTED SMOOTHING AND RE-ASSIGNMENT BLOCK
                         **************************************************************************/
                        any_smoothing_this_frame = true;
                        size_t const interval_size = track.processed_frames_history.size();

                        if (logger_) {
                            logger_->info("SMOOTH_BLOCK START g={} | interval=[{}, {}] | size={}",
                                          group_id, track.anchor_frames.front().getValue(),
                                          current_frame.getValue(), interval_size);
                        }

                        if (interval_size <= 1 || !assigner_ || !track.filter->supportsBackwardPrediction()) {
                            if (logger_) logger_->warn("SMOOTH_BLOCK SKIP g={} | interval too small or backward prediction not supported. Applying standard smoothing.", group_id);
                            auto smoothed = track.filter->smooth(track.forward_pass_history);
                            if (!smoothed.empty()) {
                                // Only skip the first element if we already have results (to avoid duplication)
                                auto start_it = all_smoothed_results[group_id].empty() ? smoothed.begin() : std::next(smoothed.begin());
                                all_smoothed_results[group_id].insert(all_smoothed_results[group_id].end(), start_it, smoothed.end());
                            }
                        } else {
                            // --- STEP 1A: GENERATE A TRUE BACKWARD-FILTERED HYPOTHESIS ---
                            std::vector<FilterState> bwd_predictions(interval_size);

                            auto bwd_filter = filter_prototype_->createBackwardFilter();
                            IdentityConfidence bwd_identity_confidence;// Backward filter gets its own confidence tracker

                            bwd_filter->initialize(track.forward_pass_history.back());
                            bwd_predictions[interval_size - 1] = track.forward_pass_history.back();
                            bwd_identity_confidence.resetOnGroundTruth();// Start with high confidence at the anchor

                            for (size_t i = interval_size - 1; i-- > 0;) {
                                // Predict state for frame i from the filter's state at i+1
                                auto pred_for_i = bwd_filter->predict();

                                bwd_predictions[i] = pred_for_i;// Store the prediction

                                // Now, perform a measurement update using data from frame i to constrain the backward filter's uncertainty
                                TimeFrameIndex frame_i = track.processed_frames_history[i];
                                auto fd_it = frame_data_lookup.find(frame_i);

                                // Find best assignment for this backward prediction
                                if (fd_it != frame_data_lookup.end() && !fd_it->second.empty()) {
                                    std::vector<Observation> observations;
                                    std::map<EntityId, FeatureCache> feature_cache;
                                    for (auto const & item: fd_it->second) {
                                        observations.push_back({std::get<1>(item)});
                                        feature_cache[std::get<1>(item)] = feature_extractor_->getAllFeatures(*std::get<0>(item));
                                    }

                                    auto bwd_assign = assigner_->solve({{group_id, pred_for_i}}, observations, feature_cache);
                                    if (!bwd_assign.observation_to_prediction.empty()) {
                                        auto const & [obs_idx, pred_idx] = *bwd_assign.observation_to_prediction.begin();
                                        EntityId entity_id = observations[obs_idx].entity_id;

                                        auto cost_it = bwd_assign.assignment_costs.find(obs_idx);
                                        if (cost_it != bwd_assign.assignment_costs.end()) {
                                            bwd_identity_confidence.updateOnAssignment(cost_it->second, bwd_assign.cost_threshold);
                                        }

                                        DataType const * data = nullptr;
                                        for (auto const & item: fd_it->second) {
                                            if (std::get<1>(item) == entity_id) {
                                                data = std::get<0>(item);
                                                break;
                                            }
                                        }

                                        if (data) {
                                            Measurement m = {feature_extractor_->getFilterFeatures(*data)};
                                            // USE THE BACKWARD CONFIDENCE to scale measurement noise
                                            double noise_scale = bwd_identity_confidence.getMeasurementNoiseScale();
                                            bwd_filter->update(pred_for_i, m, noise_scale);
                                        } else {
                                            bwd_filter->initialize(pred_for_i);
                                        }
                                    } else {
                                        bwd_filter->initialize(pred_for_i);
                                    }
                                } else {
                                    bwd_filter->initialize(pred_for_i);
                                }
                            }


                            // --- STEP 1B: RE-ASSIGNMENT BY RECONCILING FORWARD & BACKWARD HYPOTHESES ---
                            std::map<TimeFrameIndex, EntityId> revised_assignments;
                            std::map<TimeFrameIndex, double> revised_confidences;

                            double const interval_duration = static_cast<double>((track.processed_frames_history.back() - track.processed_frames_history.front()).getValue());


                            for (size_t i = 0; i < interval_size; ++i) {
                                TimeFrameIndex frame = track.processed_frames_history[i];

                                if (ground_truth.count(frame) && ground_truth.at(frame).count(group_id)) {
                                    revised_assignments[frame] = ground_truth.at(frame).at(group_id);
                                    revised_confidences[frame] = 1.0;
                                    continue;
                                }

                                auto fd_it = frame_data_lookup.find(frame);
                                if (fd_it == frame_data_lookup.end() || fd_it->second.empty()) continue;

                                std::vector<Observation> observations;
                                std::map<EntityId, FeatureCache> feature_cache;
                                for (auto const & item: fd_it->second) {
                                    observations.push_back({std::get<1>(item)});
                                    feature_cache[std::get<1>(item)] = feature_extractor_->getAllFeatures(*std::get<0>(item));
                                }

                                auto get_best_pick = [&](Assignment const & a, std::vector<Observation> const & obs) -> std::optional<std::pair<EntityId, double>> {
                                    if (a.observation_to_prediction.empty()) return std::nullopt;
                                    auto it = a.observation_to_prediction.begin();
                                    auto cost_it = a.assignment_costs.find(it->first);
                                    if (cost_it == a.assignment_costs.end()) return std::nullopt;
                                    return std::make_pair(obs.at(it->first).entity_id, cost_it->second);
                                };

                                auto fwd_assign = assigner_->solve({{group_id, track.forward_prediction_history[i]}}, observations, feature_cache);
                                auto fwd_pick = get_best_pick(fwd_assign, observations);

                                std::optional<std::pair<EntityId, double>> bwd_pick;
                                auto bwd_assign = assigner_->solve({{group_id, bwd_predictions[i]}}, observations, feature_cache);
                                bwd_pick = get_best_pick(bwd_assign, observations);


                                double fwd_cov_tr = track.forward_prediction_history[i].state_covariance.trace();
                                double bwd_cov_tr = bwd_predictions[i].state_covariance.trace();

                                if (logger_) {
                                    EntityId fwd_entity = fwd_pick ? fwd_pick->first : -1;
                                    EntityId bwd_entity = bwd_pick ? bwd_pick->first : -1;
                                    logger_->debug("RECONCILE f={} g={} | FWD: entity={}, cost={:.4f}, cov_tr={:.4f} | BWD: entity={}, cost={:.4f}, cov_tr={:.4f}",
                                                   frame.getValue(), group_id, fwd_entity, fwd_pick->second, fwd_cov_tr, bwd_entity, bwd_pick->second, bwd_cov_tr);
                                }

                                // --- NEW WEIGHTED DECISION LOGIC ---
                                double forward_weight = 1.0;
                                if (interval_duration > 0) {
                                    double frame_pos = static_cast<double>((frame - track.processed_frames_history.front()).getValue());
                                    forward_weight = 1.0 - (frame_pos / interval_duration);
                                }

                                // Add a small epsilon to avoid division by zero
                                double const epsilon = 1e-9;
                                double fwd_trust = forward_weight + epsilon;
                                double bwd_trust = (1.0 - forward_weight) + epsilon;

                                // Score is uncertainty divided by our trust in the hypothesis. Lower score is better.
                                double fwd_score = fwd_cov_tr / fwd_trust;
                                double bwd_score = bwd_cov_tr / bwd_trust;

                                bool use_bwd = false;
                                if (fwd_pick && bwd_pick) {
                                    if (bwd_score < fwd_score) {
                                        use_bwd = true;
                                    }
                                } else if (bwd_pick) {
                                    use_bwd = true;
                                }


                                auto const & winner_pick = use_bwd ? bwd_pick : fwd_pick;
                                if (!winner_pick) continue;

                                revised_assignments[frame] = winner_pick->first;
                                pending_updates.replaceForFrame(group_id, frame, winner_pick->first);

                                if (logger_) {
                                    EntityId original_entity = track.assigned_entity_history[i].has_value() ? track.assigned_entity_history[i].value() : -1;
                                    if (original_entity != winner_pick->first) {
                                         logger_->info("RECONCILE_WINNER f={} g={} | winner={} chosen_entity={} (original={}) | Decision: BWD_score={:.2f} < FWD_score={:.2f}",
                                                      frame.getValue(), group_id, (use_bwd ? "BWD" : "FWD"), winner_pick->first, original_entity, bwd_score, fwd_score);
                                    }
                                }

                                IdentityConfidence temp_conf;
                                temp_conf.updateOnAssignment(winner_pick->second, assigner_->getCostThreshold());
                                revised_confidences[frame] = temp_conf.getConfidence();
                            }

                            // --- STEP 2: RE-FILTER THE INTERVAL WITH THE CORRECTED ASSIGNMENTS ---
                            // This creates a single, consistent history based on the best assignments.
                            std::vector<FilterState> corrected_history;
                            auto temp_filter = filter_prototype_->clone();
                            temp_filter->initialize(track.forward_pass_history.front());// Start from the first anchor's state
                            corrected_history.push_back(temp_filter->getState());

                            for (size_t i = 1; i < interval_size; ++i) {
                                TimeFrameIndex frame = track.processed_frames_history[i];
                                FilterState pred = temp_filter->predict();

                                if (revised_assignments.count(frame)) {
                                    EntityId entity_id = revised_assignments.at(frame);

                                    // Find the data for the revised entity in its corresponding historical frame
                                    DataType const * data = nullptr;
                                    auto const & past_frame_data_it = frame_data_lookup.find(frame);
                                    if (past_frame_data_it != frame_data_lookup.end()) {
                                        for (auto const & item: past_frame_data_it->second) {
                                            if (std::get<1>(item) == entity_id) {
                                                data = std::get<0>(item);
                                                break;
                                            }
                                        }
                                    }

                                    if (data) {
                                        Measurement m = {feature_extractor_->getFilterFeatures(*data)};
                                        // The noise scale is based on the confidence of the *winning* assignment
                                        double confidence = revised_confidences.count(frame) ? revised_confidences.at(frame) : 0.5;
                                        double noise_scale = std::pow(10.0, 2.0 * (1.0 - confidence));
                                        temp_filter->update(pred, m, noise_scale);
                                        if (logger_) {
                                            logger_->debug("RE-FILTER f={} g={} | entity={} noise_scale={:.3f} new_cov_tr={:.4f}",
                                                           frame.getValue(), group_id, entity_id, noise_scale, temp_filter->getState().state_covariance.trace());
                                        }
                                    } else {
                                        temp_filter->initialize(pred);// Coast if data not found
                                        if (logger_) logger_->warn("RE-FILTER f={} g={} | entity {} not found in frame data, coasting.", frame.getValue(), group_id, entity_id);
                                    }
                                } else {
                                    temp_filter->initialize(pred);// Coast if no assignment was made
                                    if (logger_) logger_->debug("RE-FILTER f={} g={} | no revised assignment, coasting.", frame.getValue(), group_id);
                                }
                                corrected_history.push_back(temp_filter->getState());
                            }

                            // --- STEP 3: (REGRESSION) SMOOTH THE CORRECTED HISTORY ---
                            // Now that assignments are fixed, run a standard RTS smoother to get the best state estimates.
                            auto smoothed = track.filter->smooth(corrected_history);

                            // --- STEP 4: APPLY ASSIGNMENT-AWARE COVARIANCE INFLATION ---
                            // The smoother often produces over-confident covariances. We inflate them based on the
                            // confidence of our assignment choice to better reflect the true uncertainty.
                            if (smoothed.size() == corrected_history.size()) {
                                for (size_t i = 0; i < smoothed.size(); ++i) {
                                    TimeFrameIndex frame = track.processed_frames_history[i];
                                    if (revised_confidences.count(frame)) {
                                        double conf = revised_confidences.at(frame);
                                        // Inflate covariance more for low-confidence assignments.
                                        // Example: 1.0 confidence -> 1.0x inflation. 0.5 confidence -> 1.5x inflation.
                                        double inflation_factor = 1.0 + (1.0 - conf);
                                        smoothed[i].state_covariance *= inflation_factor;
                                    }
                                }
                            }

                            // Store results, excluding the first element only if we already have results (to avoid duplication)
                            if (!smoothed.empty()) {
                                auto start_it = all_smoothed_results[group_id].empty() ? smoothed.begin() : std::next(smoothed.begin());
                                all_smoothed_results[group_id].insert(all_smoothed_results[group_id].end(), start_it, smoothed.end());
                            }
                        }


                        /**************************************************************************
                         * END OF RE-IMPLEMENTED BLOCK
                         **************************************************************************/

                        // Collapse histories to keep only the last element for continuity
                        track.forward_pass_history = {track.forward_pass_history.back()};
                        track.forward_prediction_history = {track.forward_prediction_history.back()};
                        track.processed_frames_history = {track.processed_frames_history.back()};
                        track.assigned_entity_history = {track.assigned_entity_history.back()};
                        track.anchor_frames = {current_frame};
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
    double
    getIdentityConfidence(GroupId group_id) const {
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
