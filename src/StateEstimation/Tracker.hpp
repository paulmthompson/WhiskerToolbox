#ifndef TRACKER_HPP
#define TRACKER_HPP

#include "Assignment/IAssigner.hpp"
#include "DataSource.hpp"
#include "Features/IFeatureExtractor.hpp"
#include "Filter/IFilter.hpp"

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <ranges>
#include <set>
#include <unordered_map>
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
    // Accumulates new assignments per group
    std::unordered_map<GroupId, std::vector<EntityId>> pending_additions;
    
    // Fast O(1) lookup for entities assigned during this pass
    std::unordered_set<EntityId> entities_added_this_pass;
    
    /**
     * @brief Adds a pending assignment without updating the EntityGroupManager.
     */
    void addPending(GroupId group_id, EntityId entity_id) {
        pending_additions[group_id].push_back(entity_id);
        entities_added_this_pass.insert(entity_id);
    }
    
    /**
     * @brief Flushes all pending assignments to the EntityGroupManager.
     */
    void flushToManager(EntityGroupManager & manager) {
        for (auto const & [group_id, entities] : pending_additions) {
            for (auto entity_id : entities) {
                manager.addEntityToGroup(group_id, entity_id);
            }
        }
        pending_additions.clear();
        entities_added_this_pass.clear();
    }
    
    /**
     * @brief Checks if an entity has been assigned during this pass.
     */
    bool contains(EntityId entity_id) const {
        return entities_added_this_pass.find(entity_id) != entities_added_this_pass.end();
    }
    
    /**
     * @brief Returns the set of all entities added during this pass.
     */
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
        std::map<TimeFrameIndex, std::vector<std::tuple<DataType const*, EntityId, TimeFrameIndex>>> frame_data_lookup;
        
        for (auto const & item : data_source) {
            TimeFrameIndex time = getTimeFrameIndex(item);
            if (time >= start_frame && time <= end_frame) {
                frame_data_lookup[time].emplace_back(&getData(item), getEntityId(item), time);
            }
        }

        // Initialize tracks from EntityGroupManager
        for (auto group_id : group_manager.getAllGroupIds()) {
            if (active_tracks_.find(group_id) == active_tracks_.end()) {
                active_tracks_[group_id] = TrackedGroupState<DataType>{
                        .group_id = group_id,
                        .filter = filter_prototype_->clone()};
            }
        }

        // OPTIMIZATION 1: Build initial grouped entities set ONCE
        // This avoids O(G × E_g × log E) rebuild every frame
        std::unordered_set<EntityId> initially_grouped_entities;
        for (auto group_id : group_manager.getAllGroupIds()) {
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
                : std::vector<std::tuple<DataType const*, EntityId, TimeFrameIndex>>{};

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

            // --- Predictions ---
            std::map<GroupId, FilterState> predictions;
            for (auto & [group_id, track]: active_tracks_) {
                if (track.is_active) {
                    predictions[group_id] = track.filter->predict();
                    track.frames_since_last_seen++;
                }
            }

            std::set<GroupId> updated_groups_this_frame;
            std::set<EntityId> assigned_entities_this_frame;

            // --- Ground Truth Updates & Activation ---
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
                        track.filter->update(predictions.at(group_id), measurement);
                    }
                    track.frames_since_last_seen = 0;
                    updated_groups_this_frame.insert(group_id);
                    assigned_entities_this_frame.insert(entity_id);
                }
            }

            // --- Assignment for Ungrouped Data ---
            if (assigner_) {
                std::vector<Observation> observations;
                std::map<EntityId, FeatureCache> feature_cache;

                // OPTIMIZATION 1: O(1) membership checks using cached sets
                // Avoids O(G × E_g × log E) rebuild every frame
                for (auto const & [data_ptr, entity_id, time] : all_frame_data) {
                    if (assigned_entities_this_frame.find(entity_id) == assigned_entities_this_frame.end() &&
                        initially_grouped_entities.find(entity_id) == initially_grouped_entities.end() &&
                        !pending_updates.contains(entity_id)) {
                        observations.push_back({entity_id});
                        feature_cache[entity_id] = feature_extractor_->getAllFeatures(*data_ptr);
                    }
                }

                std::vector<Prediction> prediction_list;
                for (auto const & [group_id, pred_state]: predictions) {
                    if (active_tracks_.at(group_id).is_active) {
                        prediction_list.push_back({group_id, pred_state});
                    }
                }

                if (!observations.empty() && !prediction_list.empty()) {
                    Assignment assignment = assigner_->solve(prediction_list, observations, feature_cache);

                    for (auto const & [obs_idx, pred_idx]: assignment.observation_to_prediction) {
                        auto const & obs = observations[obs_idx];
                        auto const & pred = prediction_list[pred_idx];

                        auto & track = active_tracks_.at(pred.group_id);
                        
                        // OPTIMIZATION 2: O(1) entity lookup instead of O(E) linear search
                        auto entity_it = entity_to_index.find(obs.entity_id);
                        if (entity_it == entity_to_index.end()) continue;
                        
                        DataType const * obs_data = std::get<0>(all_frame_data[entity_it->second]);

                        Measurement measurement = {feature_extractor_->getFilterFeatures(*obs_data)};
                        track.filter->update(pred.filter_state, measurement);
                        
                        // OPTIMIZATION 1: Defer update to batch flush at anchor frames
                        pending_updates.addPending(pred.group_id, obs.entity_id);

                        updated_groups_this_frame.insert(pred.group_id);
                        assigned_entities_this_frame.insert(obs.entity_id);
                        track.frames_since_last_seen = 0;
                    }
                }
            }

            // --- Finalize Frame State, Log History, and Handle Smoothing ---
            bool any_smoothing_this_frame = false;
            for (auto & [group_id, track]: active_tracks_) {
                if (!track.is_active) continue;

                // If a track was NOT updated this frame, its state is the predicted state. Commit it.
                if (updated_groups_this_frame.find(group_id) == updated_groups_this_frame.end()) {
                    track.filter->initialize(predictions.at(group_id));
                }

                track.forward_pass_history.push_back(track.filter->getState());

                // Check for smoothing trigger on new anchor frames
                bool is_anchor = (gt_frame_it != ground_truth.end() && gt_frame_it->second.count(group_id));
                if (is_anchor) {
                    if (std::find(track.anchor_frames.begin(), track.anchor_frames.end(), current_frame) == track.anchor_frames.end()) {
                        track.anchor_frames.push_back(current_frame);
                    }

                    if (track.anchor_frames.size() >= 2) {
                        auto smoothed = track.filter->smooth(track.forward_pass_history);

                        if (all_smoothed_results[group_id].empty()) {
                            all_smoothed_results[group_id] = smoothed;
                        } else {
                            all_smoothed_results[group_id].insert(
                                    all_smoothed_results[group_id].end(),
                                    std::next(smoothed.begin()),
                                    smoothed.end());
                        }

                        track.forward_pass_history.clear();
                        track.forward_pass_history.push_back(smoothed.back());
                        // Keep only the last anchor for the next interval
                        track.anchor_frames = {current_frame};
                        
                        any_smoothing_this_frame = true;
                    }
                }
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

private:
    std::unique_ptr<IFilter> filter_prototype_;
    std::unique_ptr<IFeatureExtractor<DataType>> feature_extractor_;
    std::unique_ptr<IAssigner> assigner_;
    std::unordered_map<GroupId, TrackedGroupState<DataType>> active_tracks_;
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

    // History for smoothing between anchors
    std::vector<TimeFrameIndex> anchor_frames;
    std::vector<FilterState> forward_pass_history;
};

}// namespace StateEstimation


#endif// TRACKER_HPP
