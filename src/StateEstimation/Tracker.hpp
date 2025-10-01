#ifndef TRACKER_HPP
#define TRACKER_HPP

#include "Assignment/IAssigner.hpp"
#include "Features/IFeatureExtractor.hpp"
#include "Filter/IFilter.hpp"

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
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
    using DataSource = std::map<FrameIndex, std::vector<DataType>>;
    using GroupMap = std::map<GroupId, std::vector<EntityID>>;
    using GroundTruthMap = std::map<FrameIndex, std::map<GroupId, EntityID>>;

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
     * @brief Main processing entry point. Runs the tracking algorithm over a range of frames.
     *
     * @param data_source A map from frame index to a vector of data objects for that frame.
     * @param groups The initial group assignments. The tracker will modify this map in-place for new assignments.
     * @param ground_truth A map indicating ground-truth labels for specific groups at specific frames.
     * @param start_frame The first frame to process.
     * @param end_frame The last frame to process.
     * @param progress_callback Optional callback for progress reporting (percentage 0-100).
     * @return A map from GroupId to a vector of smoothed filter states.
     */
    SmoothedResults process(DataSource const & data_source,
                            GroupMap & groups,
                            GroundTruthMap const & ground_truth,
                            FrameIndex start_frame,
                            FrameIndex end_frame,
                            ProgressCallback progress_callback = nullptr) {

        // Initialize tracks from initial group map
        for (auto const & [group_id, entities]: groups) {
            if (active_tracks_.find(group_id) == active_tracks_.end()) {
                active_tracks_[group_id] = TrackedGroupState<DataType>{
                        .group_id = group_id,
                        .filter = filter_prototype_->clone()};
            }
        }

        SmoothedResults all_smoothed_results;

        FrameIndex const total_frames = end_frame - start_frame + 1;
        FrameIndex frames_processed = 0;

        for (FrameIndex current_frame = start_frame; current_frame <= end_frame; ++current_frame) {
            auto frame_data_it = data_source.find(current_frame);
            auto const & all_frame_data = (frame_data_it != data_source.end()) ? frame_data_it->second : std::vector<DataType>{};

            // Report progress
            if (progress_callback) {
                ++frames_processed;
                int const percentage = static_cast<int>((frames_processed * 100) / total_frames);
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
            std::set<EntityID> assigned_entities_this_frame;

            // --- Ground Truth Updates & Activation ---
            auto gt_frame_it = ground_truth.find(current_frame);
            if (gt_frame_it != ground_truth.end()) {
                for (auto const & [group_id, entity_id]: gt_frame_it->second) {
                    auto track_it = active_tracks_.find(group_id);
                    if (track_it == active_tracks_.end()) continue;
                    auto & track = track_it->second;

                    DataType const * gt_item = nullptr;
                    for (auto const & item: all_frame_data) {
                        if (item.id == entity_id) {
                            gt_item = &item;
                            break;
                        }
                    }
                    if (!gt_item) continue;

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
                std::map<EntityID, FeatureCache> feature_cache;

                std::set<EntityID> all_grouped_entities;
                for (auto const & pair: groups) {
                    all_grouped_entities.insert(pair.second.begin(), pair.second.end());
                }

                for (auto const & item: all_frame_data) {
                    if (assigned_entities_this_frame.find(item.id) == assigned_entities_this_frame.end() &&
                        all_grouped_entities.find(item.id) == all_grouped_entities.end()) {
                        observations.push_back({item.id});
                        feature_cache[item.id] = feature_extractor_->getAllFeatures(item);
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
                        DataType const * obs_data = nullptr;
                        for (auto const & item: all_frame_data) {
                            if (item.id == obs.entity_id) {
                                obs_data = &item;
                                break;
                            }
                        }

                        if (obs_data) {
                            Measurement measurement = {feature_extractor_->getFilterFeatures(*obs_data)};
                            track.filter->update(pred.filter_state, measurement);
                            groups[pred.group_id].push_back(obs.entity_id);

                            updated_groups_this_frame.insert(pred.group_id);
                            assigned_entities_this_frame.insert(obs.entity_id);
                            track.frames_since_last_seen = 0;
                        }
                    }
                }
            }

            // --- Finalize Frame State, Log History, and Handle Smoothing ---
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
                    }
                }
            }
        }
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
    std::vector<FrameIndex> anchor_frames;
    std::vector<FilterState> forward_pass_history;
};

}// namespace StateEstimation


#endif// TRACKER_HPP
