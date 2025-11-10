#ifndef STATE_ESTIMATION_OUTLIER_DETECTION_HPP
#define STATE_ESTIMATION_OUTLIER_DETECTION_HPP

#include "DataSource.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Features/IFeatureExtractor.hpp"
#include "Filter/IFilter.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <vector>
#include <map>
#include <set>
#include <functional>
#include <memory>
#include <iostream>
#include <algorithm>
#include <numeric>

namespace StateEstimation {

template<typename DataType>
class OutlierDetection {
public:
    OutlierDetection(std::unique_ptr<IFilter> filter_prototype,
                     std::unique_ptr<IFeatureExtractor<DataType>> feature_extractor,
                     CostFunction cost_function,
                     double mad_threshold = 5.0,
                     bool verbose = false)
        : filter_prototype_(std::move(filter_prototype)),
          feature_extractor_(std::move(feature_extractor)),
          cost_function_(std::move(cost_function)),
          mad_threshold_(mad_threshold),
          verbose_(verbose) {}

    void process(const std::vector<std::tuple<DataType, EntityId, TimeFrameIndex>>& data_source,
             EntityGroupManager& group_manager,
             TimeFrameIndex start_frame,
             TimeFrameIndex end_frame,
             ProgressCallback progress_callback,
             const std::vector<GroupId>& group_ids) {
    // Build a frame lookup table for efficient data access
    std::map<TimeFrameIndex, std::vector<std::pair<const DataType*, EntityId>>> frame_lookup;
    for (const auto& item : data_source) {
        TimeFrameIndex frame = std::get<2>(item);
        if (frame >= start_frame && frame <= end_frame) {
            frame_lookup[frame].emplace_back(&std::get<0>(item), std::get<1>(item));
        }
    }

    GroupId outlier_group_id = group_manager.createGroup("outlier");

    for (size_t i = 0; i < group_ids.size(); ++i) {
        GroupId group_id = group_ids[i];
        const auto entity_ids = group_manager.getEntitiesInGroup(group_id);
        std::set<EntityId> entity_set(entity_ids.begin(), entity_ids.end());

        // Extract features for the current group
        std::map<TimeFrameIndex, std::pair<EntityId, Measurement>> group_measurements;
        for (TimeFrameIndex frame = start_frame; frame <= end_frame; ++frame) {
            if (frame_lookup.count(frame)) {
                for (const auto& data_pair : frame_lookup.at(frame)) {
                    if (entity_set.count(data_pair.second)) {
                        group_measurements[frame] = {data_pair.second,
                                                     {feature_extractor_->getFilterFeatures(*data_pair.first)}};
                        break; // Assuming one entity per group per frame
                    }
                }
            }
        }

        if (group_measurements.empty()) {
            continue;
        }

        // Forward filter pass
        auto filter = filter_prototype_->clone();
        std::vector<FilterState> forward_states;
        bool initialized = false;
        for (TimeFrameIndex frame = start_frame; frame <= end_frame; ++frame) {
            if (group_measurements.count(frame)) {
                if (!initialized) {
                    // Find the first valid data point to initialize the filter
                    const DataType* first_data = nullptr;
                    for (const auto& item : data_source) {
                        if (std::get<2>(item) == group_measurements.begin()->first) {
                            first_data = &std::get<0>(item);
                            break;
                        }
                    }
                    if (first_data) {
                        filter->initialize(feature_extractor_->getInitialState(*first_data));
                        initialized = true;
                    } else {
                        continue; // Should not happen if group_measurements is not empty
                    }
                }
                auto predicted_state = filter->predict();
                auto updated_state = filter->update(predicted_state, group_measurements.at(frame).second);
                forward_states.push_back(updated_state);
            }
        }

        if (forward_states.size() < 2) {
            continue;
        }

        // Backward smoothing pass
        auto smoothed_states = filter->smooth(forward_states);

        // Calculate costs and MAD
        // Skip first frame(s) as warmup - they are used for filter initialization
        // and may have artificially high costs even after smoothing
        constexpr size_t kWarmupFrames = 3;  // Skip the first frame used for initialization
        
        std::vector<double> costs;
        std::map<TimeFrameIndex, double> costs_by_frame;
        size_t state_idx = 0;
        for (TimeFrameIndex frame = start_frame; frame <= end_frame; ++frame) {
            if (group_measurements.count(frame)) {
                if (state_idx < smoothed_states.size()) {
                    // Skip warmup frames from outlier detection
                    if (state_idx >= kWarmupFrames) {
                        double cost = cost_function_(smoothed_states[state_idx],
                                                     group_measurements.at(frame).second.feature_vector, 1.0);
                        costs.push_back(cost);
                        costs_by_frame[frame] = cost;
                    }
                    state_idx++;
                }
            }
        }

        if (costs.empty()) {
            continue;
        }

        // For Mahalanobis distance, we compare against chi-squared thresholds
        // The cost function returns sqrt(d^2), so we square it to get the actual
        // chi-squared distributed value for proper statistical interpretation
        // Common chi-squared thresholds (for ~3 DOF):
        //   - 7.81: 95% confidence (5% false positive rate)
        //   - 11.34: 99% confidence (1% false positive rate)
        //   - 16.27: 99.9% confidence (0.1% false positive rate)
        // User's mad_threshold_ parameter is the chi-squared threshold value
        
        double chi_squared_threshold = mad_threshold_;
        
        // Also compute statistics for reporting
        std::vector<double> sorted_costs = costs;
        std::sort(sorted_costs.begin(), sorted_costs.end());
        
        // Convert to chi-squared (squared Mahalanobis distance) for statistics
        std::vector<double> chi_squared_costs;
        chi_squared_costs.reserve(costs.size());
        for (double cost : costs) {
            chi_squared_costs.push_back(cost * cost);
        }
        std::sort(chi_squared_costs.begin(), chi_squared_costs.end());
        
        double median_mahalanobis = sorted_costs[sorted_costs.size() / 2];
        double median_chi_squared = chi_squared_costs[chi_squared_costs.size() / 2];
        double mean_chi_squared = std::accumulate(chi_squared_costs.begin(), chi_squared_costs.end(), 0.0) / chi_squared_costs.size();

        if (verbose_) {
            std::cout << "  Group " << group_id << ":" << std::endl;
            std::cout << "    Median Mahalanobis distance = " << median_mahalanobis << std::endl;
            std::cout << "    Median chi-squared = " << median_chi_squared 
                      << ", mean chi-squared = " << mean_chi_squared << std::endl;
            std::cout << "    Threshold (chi-squared) = " << chi_squared_threshold << std::endl;
            std::cout << "    Chi-squared range: [" << chi_squared_costs.front() << ", " << chi_squared_costs.back() << "]" << std::endl;
        }

        // Identify outliers using chi-squared threshold
        int outlier_count = 0;
        for (const auto& pair : costs_by_frame) {
            double chi_squared = pair.second * pair.second;  // Square the Mahalanobis distance
            if (chi_squared > chi_squared_threshold) {
                EntityId outlier_entity_id = group_measurements.at(pair.first).first;
                group_manager.addEntityToGroup(outlier_group_id, outlier_entity_id);
                outlier_count++;
                if (verbose_) {
                    std::cout << "    Outlier at frame " << pair.first.getValue() 
                              << ": Mahalanobis distance = " << pair.second
                              << ", chi-squared = " << chi_squared
                              << " (entity " << outlier_entity_id.id << ")" << std::endl;
                }
            }
        }
        
        if (verbose_) {
            std::cout << "  Found " << outlier_count << " outliers out of " << costs.size() << " measurements" << std::endl;
        }

        if (progress_callback) {
            progress_callback(static_cast<int>(((i + 1) * 100) / group_ids.size()));
        }
    }
}

private:
    std::unique_ptr<IFilter> filter_prototype_;
    std::unique_ptr<IFeatureExtractor<DataType>> feature_extractor_;
    CostFunction cost_function_;
    double mad_threshold_;
    bool verbose_;
};

} // namespace StateEstimation

#endif // STATE_ESTIMATION_OUTLIER_DETECTION_HPP
