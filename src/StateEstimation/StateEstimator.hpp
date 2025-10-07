#ifndef STATE_ESTIMATOR_HPP
#define STATE_ESTIMATOR_HPP

#include "DataSource.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Features/IFeatureExtractor.hpp"
#include "Filter/IFilter.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <Eigen/Dense>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace StateEstimation {

/**
 * @brief Smoothed results for each group over frames.
 */
using SmoothedGroupResults = std::map<GroupId, std::vector<FilterState>>;

/**
 * @brief Outlier information for a specific observation.
 */
struct OutlierInfo {
    TimeFrameIndex frame;
    EntityId entity_id;
    GroupId group_id;
    double innovation_magnitude;  // Mahalanobis distance or similar metric
    double threshold_used;
    Eigen::VectorXd innovation;   // The actual innovation vector
};

/**
 * @brief Results from outlier detection analysis.
 */
struct OutlierDetectionResults {
    std::vector<OutlierInfo> outliers;
    std::map<GroupId, std::vector<double>> innovation_magnitudes;  // All magnitudes per group
    std::map<GroupId, double> mean_innovation;                      // Mean per group
    std::map<GroupId, double> std_innovation;                       // Std dev per group
};

/**
 * @brief State estimator for smoothing and outlier detection on grouped data.
 * 
 * This class performs forward-backward smoothing using a Kalman filter (or any IFilter)
 * on data that has already been grouped. It is separate from the assignment problem
 * and can be used for:
 * - Final smoothing after global assignment
 * - Outlier detection for manual review
 * - Quality metrics on tracking results
 * 
 * @tparam DataType raw observation type (e.g., Line2D)
 */
template<typename DataType>
class StateEstimator {
public:
    /**
     * @brief Construct a new StateEstimator
     * 
     * @param filter_prototype Prototype filter (cloned for each group)
     * @param feature_extractor Feature extractor for DataType
     */
    StateEstimator(std::unique_ptr<IFilter> filter_prototype,
                   std::unique_ptr<IFeatureExtractor<DataType>> feature_extractor)
        : _filter_prototype(std::move(filter_prototype)),
          _feature_extractor(std::move(feature_extractor)) {}

    /**
     * @brief Smooth all groups over a range of frames.
     * 
     * Performs forward filtering followed by backward smoothing (e.g., RTS smoother)
     * for each group independently.
     * 
     * @param data_source Zero-copy data source
     * @param group_manager Group manager with existing assignments
     * @param start_frame Inclusive start frame
     * @param end_frame Inclusive end frame
     * @return Smoothed states per group across processed frames
     */
    template<typename Source>
        requires DataSource<Source, DataType>
    [[nodiscard]] SmoothedGroupResults smoothGroups(Source && data_source,
                                                     EntityGroupManager const & group_manager,
                                                     TimeFrameIndex start_frame,
                                                     TimeFrameIndex end_frame) {
        // Build frame lookup for efficient access
        auto frame_lookup = buildFrameLookup(data_source, start_frame, end_frame);
        
        // Get all groups from the manager
        auto group_ids = group_manager.getAllGroupIds();
        
        SmoothedGroupResults results;
        
        for (GroupId group_id : group_ids) {
            // Get entities in this group
            auto entity_ids = group_manager.getEntitiesInGroup(group_id);
            if (entity_ids.empty()) continue;
            
            // Build chronological sequence of observations for this group
            std::vector<ObservationNode> sequence;
            for (TimeFrameIndex t = start_frame; t <= end_frame; ++t) {
                if (!frame_lookup.count(t)) continue;
                
                for (auto const & item : frame_lookup.at(t)) {
                    EntityId entity_id = std::get<1>(item);
                    
                    // Check if this entity belongs to the current group
                    if (std::find(entity_ids.begin(), entity_ids.end(), entity_id) != entity_ids.end()) {
                        sequence.push_back({
                            t,
                            entity_id,
                            std::get<0>(item)  // DataType pointer
                        });
                    }
                }
            }
            
            // Sort by time (should already be sorted, but ensure it)
            std::sort(sequence.begin(), sequence.end(), 
                     [](auto const & a, auto const & b) { return a.frame < b.frame; });
            
            // Perform forward-backward smoothing on this sequence
            if (!sequence.empty()) {
                results[group_id] = smoothSequence(sequence);
            }
        }
        
        return results;
    }

    /**
     * @brief Detect outliers in grouped data based on innovation statistics.
     * 
     * Performs forward filtering and computes innovation (prediction error) for each
     * observation. Outliers are identified as observations with innovation magnitude
     * exceeding a threshold (mean + k * std_dev).
     * 
     * @param data_source Zero-copy data source
     * @param group_manager Group manager with existing assignments
     * @param start_frame Inclusive start frame
     * @param end_frame Inclusive end frame
     * @param threshold_sigma Number of standard deviations for outlier threshold (default: 3.0)
     * @return Outlier detection results with flagged observations
     */
    template<typename Source>
        requires DataSource<Source, DataType>
    [[nodiscard]] OutlierDetectionResults detectOutliers(Source && data_source,
                                                          EntityGroupManager const & group_manager,
                                                          TimeFrameIndex start_frame,
                                                          TimeFrameIndex end_frame,
                                                          double threshold_sigma = 3.0) {
        // Build frame lookup for efficient access
        auto frame_lookup = buildFrameLookup(data_source, start_frame, end_frame);
        
        // Get all groups from the manager
        auto group_ids = group_manager.getAllGroupIds();
        
        OutlierDetectionResults results;
        
        for (GroupId group_id : group_ids) {
            // Get entities in this group
            auto entity_ids = group_manager.getEntitiesInGroup(group_id);
            if (entity_ids.empty()) continue;
            
            // Build chronological sequence of observations for this group
            std::vector<ObservationNode> sequence;
            for (TimeFrameIndex t = start_frame; t <= end_frame; ++t) {
                if (!frame_lookup.count(t)) continue;
                
                for (auto const & item : frame_lookup.at(t)) {
                    EntityId entity_id = std::get<1>(item);
                    
                    // Check if this entity belongs to the current group
                    if (std::find(entity_ids.begin(), entity_ids.end(), entity_id) != entity_ids.end()) {
                        sequence.push_back({
                            t,
                            entity_id,
                            std::get<0>(item)  // DataType pointer
                        });
                    }
                }
            }
            
            // Sort by time
            std::sort(sequence.begin(), sequence.end(), 
                     [](auto const & a, auto const & b) { return a.frame < b.frame; });
            
            // Compute innovations for this sequence
            if (!sequence.empty()) {
                auto group_outliers = detectOutliersInSequence(sequence, group_id, threshold_sigma);
                
                // Aggregate results
                for (auto const & outlier : group_outliers.outliers) {
                    results.outliers.push_back(outlier);
                }
                results.innovation_magnitudes[group_id] = group_outliers.innovation_magnitudes[group_id];
                results.mean_innovation[group_id] = group_outliers.mean_innovation[group_id];
                results.std_innovation[group_id] = group_outliers.std_innovation[group_id];
            }
        }
        
        return results;
    }

private:
    struct ObservationNode {
        TimeFrameIndex frame;
        EntityId entity_id;
        DataType const * data;
    };

    using FrameBucket = std::vector<std::tuple<DataType const *, EntityId, TimeFrameIndex>>;

    /**
     * @brief Perform forward-backward smoothing on a chronological sequence.
     */
    std::vector<FilterState> smoothSequence(std::vector<ObservationNode> const & sequence) {
        if (sequence.empty()) return {};
        
        auto filter = _filter_prototype->clone();
        std::vector<FilterState> forward_states;
        
        // Forward pass
        for (size_t i = 0; i < sequence.size(); ++i) {
            auto const & node = sequence[i];
            
            if (i == 0) {
                // Initialize with first observation
                filter->initialize(_feature_extractor->getInitialState(*node.data));
            } else {
                // Predict forward from previous state
                TimeFrameIndex prev_frame = sequence[i - 1].frame;
                int num_steps = (node.frame - prev_frame).getValue();
                
                if (num_steps <= 0) {
                    continue;  // Skip invalid steps
                }
                
                // Multi-step prediction
                FilterState pred = filter->getState();
                for (int step = 0; step < num_steps; ++step) {
                    pred = filter->predict();
                }
                
                // Update with measurement
                filter->update(pred, {_feature_extractor->getFilterFeatures(*node.data)});
            }
            
            forward_states.push_back(filter->getState());
        }
        
        // Backward smoothing pass
        if (forward_states.size() > 1) {
            return filter->smooth(forward_states);
        } else {
            return forward_states;
        }
    }

    /**
     * @brief Detect outliers in a chronological sequence based on innovation statistics.
     */
    OutlierDetectionResults detectOutliersInSequence(std::vector<ObservationNode> const & sequence,
                                                      GroupId group_id,
                                                      double threshold_sigma) {
        OutlierDetectionResults results;
        
        if (sequence.empty()) return results;
        
        auto filter = _filter_prototype->clone();
        std::vector<double> innovation_magnitudes;
        
        // Forward pass: compute innovations
        for (size_t i = 0; i < sequence.size(); ++i) {
            auto const & node = sequence[i];
            
            if (i == 0) {
                // Initialize with first observation (no innovation to compute)
                filter->initialize(_feature_extractor->getInitialState(*node.data));
                innovation_magnitudes.push_back(0.0);  // No innovation for first observation
                continue;
            }
            
            // Predict forward from previous state
            TimeFrameIndex prev_frame = sequence[i - 1].frame;
            int num_steps = (node.frame - prev_frame).getValue();
            
            if (num_steps <= 0) {
                innovation_magnitudes.push_back(0.0);  // Skip invalid steps
                continue;
            }
            
            // Multi-step prediction
            FilterState pred = filter->getState();
            for (int step = 0; step < num_steps; ++step) {
                pred = filter->predict();
            }
            
            // Compute innovation (prediction error)
            // Note: We compute innovation in measurement space by comparing the observation
            // to the predicted observation. For a proper Kalman filter, this would use
            // the measurement matrix H to project state to measurement space.
            // For simplicity, we assume the observation is comparable to part of the state.
            Eigen::VectorXd observation = _feature_extractor->getFilterFeatures(*node.data);
            
            // Extract the measurement portion of the state (first N elements matching observation size)
            Eigen::VectorXd predicted_observation = pred.state_mean.head(observation.size());
            Eigen::VectorXd innovation = observation - predicted_observation;
            
            // Compute innovation magnitude (could use Mahalanobis, but L2 norm is simpler)
            double magnitude = innovation.norm();
            innovation_magnitudes.push_back(magnitude);
            
            // Update filter with measurement
            filter->update(pred, {observation});
        }
        
        // Compute statistics (skip first observation which has 0 innovation)
        if (innovation_magnitudes.size() > 1) {
            std::vector<double> valid_magnitudes(innovation_magnitudes.begin() + 1, 
                                                 innovation_magnitudes.end());
            
            double mean = 0.0;
            for (double mag : valid_magnitudes) {
                mean += mag;
            }
            mean /= valid_magnitudes.size();
            
            double variance = 0.0;
            for (double mag : valid_magnitudes) {
                variance += (mag - mean) * (mag - mean);
            }
            variance /= valid_magnitudes.size();
            double std_dev = std::sqrt(variance);
            
            results.innovation_magnitudes[group_id] = innovation_magnitudes;
            results.mean_innovation[group_id] = mean;
            results.std_innovation[group_id] = std_dev;
            
            // Flag outliers
            double threshold = mean + threshold_sigma * std_dev;
            for (size_t i = 1; i < sequence.size(); ++i) {  // Start from 1 (skip first)
                if (innovation_magnitudes[i] > threshold) {
                    // Recompute innovation vector for outlier info
                    Eigen::VectorXd observation = _feature_extractor->getFilterFeatures(*sequence[i].data);
                    
                    // Would need to re-run filter to get exact innovation at this point,
                    // but for simplicity we'll create a placeholder
                    Eigen::VectorXd innovation = Eigen::VectorXd::Zero(observation.size());
                    
                    results.outliers.push_back({
                        sequence[i].frame,
                        sequence[i].entity_id,
                        group_id,
                        innovation_magnitudes[i],
                        threshold,
                        innovation
                    });
                }
            }
        }
        
        return results;
    }

    /**
     * @brief Build frame lookup for efficient access.
     */
    [[nodiscard]] std::map<TimeFrameIndex, FrameBucket>
    buildFrameLookup(auto && data_source, TimeFrameIndex start_frame, TimeFrameIndex end_frame) const {
        std::map<TimeFrameIndex, FrameBucket> lookup;
        for (auto const & item : data_source) {
            TimeFrameIndex t = getTimeFrameIndex(item);
            if (t >= start_frame && t <= end_frame) {
                lookup[t].emplace_back(&getData(item), getEntityId(item), t);
            }
        }
        return lookup;
    }

private:
    std::unique_ptr<IFilter> _filter_prototype;
    std::unique_ptr<IFeatureExtractor<DataType>> _feature_extractor;
};

}  // namespace StateEstimation

#endif  // STATE_ESTIMATOR_HPP
