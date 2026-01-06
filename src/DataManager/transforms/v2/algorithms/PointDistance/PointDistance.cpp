#include "PointDistance.hpp"

#include "CoreGeometry/points.hpp"
#include "Points/Point_Data.hpp"
#include "transforms/v2/core/ComputeContext.hpp"

#include <cmath>
#include <deque>
#include <map>

namespace WhiskerToolbox::Transforms::V2::PointDistance {

namespace {
    // Helper function to calculate euclidean distance
    float euclideanDistance(Point2D<float> const & p1, Point2D<float> const & p2) {
        float dx = p1.x - p2.x;
        float dy = p1.y - p2.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    // Helper function to calculate global average point
    Point2D<float> calculateGlobalAverage(PointData const & point_data) {
        float sum_x = 0.0f;
        float sum_y = 0.0f;
        int count = 0;

        for (auto const & [time, entity_id, point] : point_data.flattened_data()) {
            (void)time;
            (void)entity_id;
            sum_x += point.x;
            sum_y += point.y;
            ++count;
        }

        if (count == 0) {
            return Point2D<float>{0.0f, 0.0f};
        }

        return Point2D<float>{sum_x / count, sum_y / count};
    }
}

std::vector<PointDistanceResult> calculatePointDistance(
        PointData const & point_data,
        PointDistanceParams const & params,
        PointData const * reference_point_data) {

    std::vector<PointDistanceResult> results;

    switch (params.reference_type) {
        case ReferenceType::GlobalAverage: {
            // Calculate the global average point
            Point2D<float> avg_point = calculateGlobalAverage(point_data);

            // Calculate distance from each point to the global average
            for (auto const & [time, entity_id, point] : point_data.flattened_data()) {
                (void)entity_id;
                float dist = euclideanDistance(point, avg_point);
                results.push_back({static_cast<int>(time.getValue()), dist});
            }
            break;
        }

        case ReferenceType::RollingAverage: {
            int window_size = params.getWindowSize();

            // Build a map of time to points for easier access
            std::map<int, std::vector<Point2D<float>>> time_to_points;
            for (auto const & [time, entity_id, point] : point_data.flattened_data()) {
                (void)entity_id;
                time_to_points[static_cast<int>(time.getValue())].push_back(point);
            }

            // Process each time point
            for (auto const & [current_time, points] : time_to_points) {
                // Calculate rolling average over the window
                float sum_x = 0.0f;
                float sum_y = 0.0f;
                int count = 0;

                int window_start = std::max(0, current_time - window_size / 2);
                int window_end = current_time + window_size / 2;

                for (int t = window_start; t <= window_end; ++t) {
                    auto it = time_to_points.find(t);
                    if (it != time_to_points.end()) {
                        for (auto const & p : it->second) {
                            sum_x += p.x;
                            sum_y += p.y;
                            ++count;
                        }
                    }
                }

                if (count > 0) {
                    Point2D<float> avg_point{sum_x / count, sum_y / count};

                    // Calculate distance for all points at current time
                    for (auto const & point : points) {
                        float dist = euclideanDistance(point, avg_point);
                        results.push_back({current_time, dist});
                    }
                }
            }
            break;
        }

        case ReferenceType::SetPoint: {
            Point2D<float> ref_point{params.getReferenceX(), params.getReferenceY()};

            // Calculate distance from each point to the set point
            for (auto const & [time, entity_id, point] : point_data.flattened_data()) {
                (void)entity_id;
                float dist = euclideanDistance(point, ref_point);
                results.push_back({static_cast<int>(time.getValue()), dist});
            }
            break;
        }

        case ReferenceType::OtherPointData: {
            if (!reference_point_data) {
                // No reference data provided, return empty results
                break;
            }

            // Build a map of time to reference points
            std::map<int, std::vector<Point2D<float>>> ref_time_to_points;
            for (auto const & [time, entity_id, point] : reference_point_data->flattened_data()) {
                (void)entity_id;
                ref_time_to_points[static_cast<int>(time.getValue())].push_back(point);
            }

            // Calculate distance between corresponding points
            for (auto const & [time, entity_id, point] : point_data.flattened_data()) {
                (void)entity_id;
                int time_val = static_cast<int>(time.getValue());

                auto it = ref_time_to_points.find(time_val);
                if (it != ref_time_to_points.end() && !it->second.empty()) {
                    // Use the first point if multiple exist at this time
                    // You could also average them or use closest point
                    Point2D<float> ref_point = it->second[0];
                    float dist = euclideanDistance(point, ref_point);
                    results.push_back({time_val, dist});
                }
            }
            break;
        }
    }

    return results;
}

std::vector<PointDistanceResult> calculatePointDistanceWithContext(
        PointData const & point_data,
        PointDistanceParams const & params,
        ComputeContext const & ctx,
        PointData const * reference_point_data) {

    // Count total points for progress reporting
    int total_points = 0;
    for (auto const & [time, entity_id, point] : point_data.flattened_data()) {
        (void)time; (void)entity_id; (void)point;
        ++total_points;
    }

    std::vector<PointDistanceResult> results;
    int processed = 0;

    // Similar logic to calculatePointDistance but with progress reporting
    switch (params.reference_type) {
        case ReferenceType::GlobalAverage: {
            Point2D<float> avg_point = calculateGlobalAverage(point_data);

            for (auto const & [time, entity_id, point] : point_data.flattened_data()) {
                if (ctx.shouldCancel()) {
                    throw std::runtime_error("Computation cancelled");
                }

                (void)entity_id;
                float dist = euclideanDistance(point, avg_point);
                results.push_back({static_cast<int>(time.getValue()), dist});

                ++processed;
                if (total_points > 0) {
                    ctx.reportProgress((processed * 100) / total_points);
                }
            }
            break;
        }

        case ReferenceType::RollingAverage: {
            int window_size = params.getWindowSize();

            std::map<int, std::vector<Point2D<float>>> time_to_points;
            for (auto const & [time, entity_id, point] : point_data.flattened_data()) {
                (void)entity_id;
                time_to_points[static_cast<int>(time.getValue())].push_back(point);
            }

            int time_points_processed = 0;
            int total_time_points = static_cast<int>(time_to_points.size());

            for (auto const & [current_time, points] : time_to_points) {
                if (ctx.shouldCancel()) {
                    throw std::runtime_error("Computation cancelled");
                }

                float sum_x = 0.0f;
                float sum_y = 0.0f;
                int count = 0;

                int window_start = std::max(0, current_time - window_size / 2);
                int window_end = current_time + window_size / 2;

                for (int t = window_start; t <= window_end; ++t) {
                    auto it = time_to_points.find(t);
                    if (it != time_to_points.end()) {
                        for (auto const & p : it->second) {
                            sum_x += p.x;
                            sum_y += p.y;
                            ++count;
                        }
                    }
                }

                if (count > 0) {
                    Point2D<float> avg_point{sum_x / count, sum_y / count};

                    for (auto const & point : points) {
                        float dist = euclideanDistance(point, avg_point);
                        results.push_back({current_time, dist});
                    }
                }

                ++time_points_processed;
                if (total_time_points > 0) {
                    ctx.reportProgress((time_points_processed * 100) / total_time_points);
                }
            }
            break;
        }

        case ReferenceType::SetPoint: {
            Point2D<float> ref_point{params.getReferenceX(), params.getReferenceY()};

            for (auto const & [time, entity_id, point] : point_data.flattened_data()) {
                if (ctx.shouldCancel()) {
                    throw std::runtime_error("Computation cancelled");
                }

                (void)entity_id;
                float dist = euclideanDistance(point, ref_point);
                results.push_back({static_cast<int>(time.getValue()), dist});

                ++processed;
                if (total_points > 0) {
                    ctx.reportProgress((processed * 100) / total_points);
                }
            }
            break;
        }

        case ReferenceType::OtherPointData: {
            if (!reference_point_data) {
                break;
            }

            std::map<int, std::vector<Point2D<float>>> ref_time_to_points;
            for (auto const & [time, entity_id, point] : reference_point_data->flattened_data()) {
                (void)entity_id;
                ref_time_to_points[static_cast<int>(time.getValue())].push_back(point);
            }

            for (auto const & [time, entity_id, point] : point_data.flattened_data()) {
                if (ctx.shouldCancel()) {
                    throw std::runtime_error("Computation cancelled");
                }

                (void)entity_id;
                int time_val = static_cast<int>(time.getValue());

                auto it = ref_time_to_points.find(time_val);
                if (it != ref_time_to_points.end() && !it->second.empty()) {
                    Point2D<float> ref_point = it->second[0];
                    float dist = euclideanDistance(point, ref_point);
                    results.push_back({time_val, dist});
                }

                ++processed;
                if (total_points > 0) {
                    ctx.reportProgress((processed * 100) / total_points);
                }
            }
            break;
        }
    }

    return results;
}

}// namespace WhiskerToolbox::Transforms::V2::PointDistance

