#include "line_alignment.hpp"

#include "Lines/Line_Data.hpp"
#include "Media/Media_Data.hpp"
#include "CoreGeometry/line_geometry.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityTypes.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <vector>


template<typename T>
T get_pixel_value(Point2D<float> const & point,
                  std::vector<T> const & image_data,
                  ImageSize const & image_size) {
    int x = static_cast<int>(std::round(point.x));
    int y = static_cast<int>(std::round(point.y));

    // Check bounds
    if (x < 0 || x >= image_size.width || y < 0 || y >= image_size.height) {
        return 0;
    }

    size_t index = static_cast<size_t>(y * image_size.width + x);
    //size_t index = static_cast<size_t>(x * image_size.height + y);
    if (index < image_data.size()) {
        return image_data[index];
    }

    return 0;
}

// Calculate FWHM center point for a single vertex
template<typename T>
Point2D<float> calculate_fwhm_center(Point2D<float> const & vertex,
                                     Point2D<float> const & perpendicular_dir,
                                     int width,
                                     int perpendicular_range,
                                     std::vector<T> const & image_data,
                                     ImageSize const & image_size,
                                     FWHMApproach /*approach*/) {
    if (width <= 0) {
        return vertex;// Return original vertex if no width
    }

    std::vector<Point2D<float>> center_points;
    std::vector<float> intensities;
    std::vector<T> profile = std::vector<T>(perpendicular_range+1, 0);
    std::vector<float> distances = std::vector<float>(perpendicular_range+1, 0);

    // Sample multiple points along the width of the analysis strip
    for (int w = -width / 2; w <= width / 2; ++w) {
        // Calculate the sampling point along the width direction
        Point2D<float> width_dir{-perpendicular_dir.y, perpendicular_dir.x};// Perpendicular to perp dir

        Point2D<float> sample_start = {
                vertex.x + width_dir.x * static_cast<float>(w),
                vertex.y + width_dir.y * static_cast<float>(w)};

        // Sample intensity profile along the perpendicular direction
        profile.assign(perpendicular_range+1, 0);
        distances.assign(perpendicular_range+1, 0);

        // Sample up to perpendicular_range pixels in each direction along the perpendicular
        int index = 0;
        for (int d = -perpendicular_range / 2; d <= perpendicular_range / 2; ++d) {
            Point2D<float> sample_point = {
                    sample_start.x + perpendicular_dir.x * static_cast<float>(d),
                    sample_start.y + perpendicular_dir.y * static_cast<float>(d)};

            T intensity = get_pixel_value(sample_point, image_data, image_size);
            profile[index] = intensity;
            distances[index] = static_cast<float>(d);
            index++;
        }

        // Find the maximum intensity in the profile
        auto max_it = std::max_element(profile.begin(), profile.end());
        if (max_it == profile.end()) {
            continue;
        }

        T max_intensity = *max_it;
        if (max_intensity == 0) {
            continue;// Skip if no signal
        }

        // Find all positions with the maximum intensity
        std::vector<int> max_indices;
        for (size_t i = 0; i < profile.size(); ++i) {
            if (profile[i] == max_intensity) {
                max_indices.push_back(static_cast<int>(i));
            }
        }

        // Calculate the average position of all maximum points
        float avg_max_index = 0.0f;
        for (int index: max_indices) {
            avg_max_index += static_cast<float>(index);
        }
        avg_max_index /= static_cast<float>(max_indices.size());

        int max_offset = static_cast<int>(std::round(avg_max_index)) - perpendicular_range / 2;

        // Find minimum in profile
        auto min_it = std::min_element(profile.begin(), profile.end());
        if (min_it == profile.end()) {
            continue;
        }

        T min_intensity = *min_it;

        // Find the half-maximum threshold
        float half_max = (static_cast<float>(max_intensity) + static_cast<float>(min_intensity)) / 2.0f;

        // Find the left and right boundaries at half maximum, starting from the average max position
        int left_bound = max_offset;
        int right_bound = max_offset;

        // Work leftward from the average max position to find the first half-maximum
        int avg_max_index_int = static_cast<int>(std::round(avg_max_index));
        for (int i = avg_max_index_int; i >= 0; --i) {
            if (profile[static_cast<size_t>(i)] < half_max) {
                left_bound = i + 1 - perpendicular_range / 2;// +1 to include the last point >= half_max
                break;
            }
        }

        // Work rightward from the average max position to find the first half-maximum
        for (size_t i = static_cast<size_t>(avg_max_index_int); i < profile.size(); ++i) {
            if (profile[i] < half_max) {
                right_bound = static_cast<int>(i) - 1 - perpendicular_range / 2;// -1 to include the last point >= half_max
                break;
            }
        }

        // Calculate the center point of the FWHM region
        float center_offset = (static_cast<float>(left_bound) + static_cast<float>(right_bound)) / 2.0f;
        Point2D<float> center_point = {
                sample_start.x + perpendicular_dir.x * center_offset,
                sample_start.y + perpendicular_dir.y * center_offset};
        center_points.push_back(center_point);
        intensities.push_back(static_cast<float>(max_intensity));
    }

    // Calculate weighted average center point based on intensity
    if (center_points.empty()) {
        return vertex;// Return original vertex if no center points found
    }

    float total_weight = 0.0f;
    Point2D<float> weighted_sum{0.0f, 0.0f};

    for (size_t i = 0; i < center_points.size(); ++i) {
        float weight = intensities[i];
        weighted_sum.x += center_points[i].x * weight;
        weighted_sum.y += center_points[i].y * weight;
        total_weight += weight;
    }

    if (total_weight > 0) {
        return Point2D<float>{weighted_sum.x / total_weight, weighted_sum.y / total_weight};
    }

    return vertex;// Return original vertex if no valid weights
}

// Calculate FWHM profile extents for a single vertex
template<typename T>
Line2D calculate_fwhm_profile_extents(Point2D<float> const & vertex,
                                      Point2D<float> const & perpendicular_dir,
                                      int width,
                                      int perpendicular_range,
                                      std::vector<T> const & image_data,
                                      ImageSize const & image_size,
                                      FWHMApproach /*approach*/) {
    if (width <= 0) {
        // Return a line with just the original vertex repeated
        Line2D debug_line;
        debug_line.push_back(vertex);
        debug_line.push_back(vertex);
        debug_line.push_back(vertex);
        return debug_line;
    }

    std::vector<Point2D<float>> left_extents;
    std::vector<Point2D<float>> right_extents;
    std::vector<Point2D<float>> max_points;
    std::vector<float> intensities;

    std::vector<T> profile = std::vector<T>(perpendicular_range+1, 0);
    std::vector<float> distances = std::vector<float>(perpendicular_range+1, 0);

    // Sample multiple points along the width of the analysis strip
    for (int w = -width / 2; w <= width / 2; ++w) {
        // Calculate the sampling point along the width direction
        Point2D<float> width_dir{-perpendicular_dir.y, perpendicular_dir.x};// Perpendicular to perp dir

        Point2D<float> sample_start = {
                vertex.x + width_dir.x * static_cast<float>(w),
                vertex.y + width_dir.y * static_cast<float>(w)};

        // Sample intensity profile along the perpendicular direction
        profile.assign(perpendicular_range+1, 0);
        distances.assign(perpendicular_range+1, 0);

        // Sample up to perpendicular_range pixels in each direction along the perpendicular
        int index = 0;
        for (int d = -perpendicular_range / 2; d <= perpendicular_range / 2; ++d) {
            Point2D<float> sample_point = {
                    sample_start.x + perpendicular_dir.x * static_cast<float>(d),
                    sample_start.y + perpendicular_dir.y * static_cast<float>(d)};

            T intensity = get_pixel_value(sample_point, image_data, image_size);
            profile[index] = intensity;
            distances[index] = static_cast<float>(d);
            index++;
        }

        // Find the maximum intensity in the profile
        auto max_it = std::max_element(profile.begin(), profile.end());
        if (max_it == profile.end()) {
            continue;
        }

        T max_intensity = *max_it;
        if (max_intensity == 0) {
            continue;// Skip if no signal
        }

        // Find all positions with the maximum intensity
        std::vector<int> max_indices;
        for (size_t i = 0; i < profile.size(); ++i) {
            if (profile[i] == max_intensity) {
                max_indices.push_back(static_cast<int>(i));
            }
        }

        // Calculate the average position of all maximum points
        float avg_max_index = 0.0f;
        for (int index: max_indices) {
            avg_max_index += static_cast<float>(index);
        }
        avg_max_index /= static_cast<float>(max_indices.size());

        int max_offset = static_cast<int>(std::round(avg_max_index)) - perpendicular_range / 2;

        // Calculate the maximum point location
        Point2D<float> max_point = {
                sample_start.x + perpendicular_dir.x * static_cast<float>(max_offset),
                sample_start.y + perpendicular_dir.y * static_cast<float>(max_offset)};

        // Find minimum in profile
        auto min_it = std::min_element(profile.begin(), profile.end());
        if (min_it == profile.end()) {
            continue;
        }

        T min_intensity = *min_it;

        // Find the half-maximum threshold
        float half_max = (static_cast<float>(max_intensity) + static_cast<float>(min_intensity)) / 2.0f;

        // Find the left and right boundaries at half maximum, starting from the average max position
        int left_bound = max_offset;
        int right_bound = max_offset;

        // Work leftward from the average max position to find the first half-maximum
        int avg_max_index_int = static_cast<int>(std::round(avg_max_index));
        for (int i = avg_max_index_int; i >= 0; --i) {
            if (profile[static_cast<size_t>(i)] < half_max) {
                left_bound = i + 1 - perpendicular_range / 2;// +1 to include the last point >= half_max
                break;
            }
        }

        // Work rightward from the average max position to find the first half-maximum
        for (size_t i = static_cast<size_t>(avg_max_index_int); i < profile.size(); ++i) {
            if (profile[i] < half_max) {
                right_bound = static_cast<int>(i) - 1 - perpendicular_range / 2;// -1 to include the last point >= half_max
                break;
            }
        }

        // Calculate the left and right extent points
        Point2D<float> left_extent = {
                sample_start.x + perpendicular_dir.x * static_cast<float>(left_bound),
                sample_start.y + perpendicular_dir.y * static_cast<float>(left_bound)};

        Point2D<float> right_extent = {
                sample_start.x + perpendicular_dir.x * static_cast<float>(right_bound),
                sample_start.y + perpendicular_dir.y * static_cast<float>(right_bound)};

        left_extents.push_back(left_extent);
        right_extents.push_back(right_extent);
        max_points.push_back(max_point);
        intensities.push_back(static_cast<float>(max_intensity));
    }

    // Calculate weighted average extents based on intensity
    if (left_extents.empty()) {
        // Return a line with just the original vertex repeated
        Line2D debug_line;
        debug_line.push_back(vertex);
        debug_line.push_back(vertex);
        debug_line.push_back(vertex);
        return debug_line;
    }

    float total_weight = 0.0f;
    Point2D<float> weighted_left_sum{0.0f, 0.0f};
    Point2D<float> weighted_right_sum{0.0f, 0.0f};
    Point2D<float> weighted_max_sum{0.0f, 0.0f};

    for (size_t i = 0; i < left_extents.size(); ++i) {
        float weight = intensities[i];
        weighted_left_sum.x += left_extents[i].x * weight;
        weighted_left_sum.y += left_extents[i].y * weight;
        weighted_right_sum.x += right_extents[i].x * weight;
        weighted_right_sum.y += right_extents[i].y * weight;
        weighted_max_sum.x += max_points[i].x * weight;
        weighted_max_sum.y += max_points[i].y * weight;
        total_weight += weight;
    }

    if (total_weight > 0) {
        Point2D<float> avg_left = {weighted_left_sum.x / total_weight, weighted_left_sum.y / total_weight};
        Point2D<float> avg_right = {weighted_right_sum.x / total_weight, weighted_right_sum.y / total_weight};
        Point2D<float> avg_max = {weighted_max_sum.x / total_weight, weighted_max_sum.y / total_weight};

        Line2D debug_line;
        debug_line.push_back(avg_left);
        debug_line.push_back(avg_max);
        debug_line.push_back(avg_right);
        return debug_line;
    }

    // Return a line with just the original vertex repeated
    Line2D debug_line;
    debug_line.push_back(vertex);
    debug_line.push_back(vertex);
    debug_line.push_back(vertex);
    return debug_line;
}

// Helper function to process lines with image data of any type
template<typename T>
std::vector<Line2D> processLinesWithImageData(std::vector<Line2D> const & lines,
                                               std::vector<T> const & image_data,
                                               ImageSize const & image_size,
                                               int width,
                                               int perpendicular_range,
                                               FWHMApproach approach,
                                               LineAlignmentOutputMode output_mode) {
    std::vector<Line2D> aligned_lines;
    
    for (auto const & line : lines) {
        if (line.size() < 3) {
            // Skip lines with fewer than 3 vertices
            aligned_lines.push_back(line);
            continue;
        }

        if (output_mode == LineAlignmentOutputMode::FWHM_PROFILE_EXTENTS) {
            // Debug mode: create a debug line for each vertex
            for (size_t i = 0; i < line.size(); ++i) {
                Point2D<float> vertex = line[i];

                // Calculate perpendicular direction
                Point2D<float> perp_dir = calculate_perpendicular_direction(line, i);

                if (perp_dir.x == 0.0f && perp_dir.y == 0.0f) {
                    // If we can't calculate a perpendicular direction, create a debug line with just the vertex repeated
                    Line2D debug_line;
                    debug_line.push_back(vertex);
                    debug_line.push_back(vertex);
                    debug_line.push_back(vertex);
                    aligned_lines.push_back(debug_line);
                    continue;
                }

                // Debug mode: calculate FWHM profile extents
                Line2D debug_line = calculate_fwhm_profile_extents(vertex,
                                                                   perp_dir,
                                                                   width,
                                                                   perpendicular_range,
                                                                   image_data,
                                                                   image_size,
                                                                   approach);
                aligned_lines.push_back(debug_line);
            }
        } else {
            // Normal mode: create a single aligned line
            Line2D aligned_line;

            // Process each vertex in the line
            for (size_t i = 0; i < line.size(); ++i) {
                Point2D<float> vertex = line[i];

                // Calculate perpendicular direction
                Point2D<float> perp_dir = calculate_perpendicular_direction(line, i);

                if (perp_dir.x == 0.0f && perp_dir.y == 0.0f) {
                    // If we can't calculate a perpendicular direction, keep the original vertex
                    aligned_line.push_back(vertex);
                    continue;
                }

                // Normal mode: calculate FWHM center point
                Point2D<float> aligned_vertex = calculate_fwhm_center(vertex,
                                                                      perp_dir,
                                                                      width,
                                                                      perpendicular_range,
                                                                      image_data,
                                                                      image_size,
                                                                      approach);

                // Ensure the aligned vertex stays within image bounds
                aligned_vertex.x = std::max(0.0f, std::min(static_cast<float>(image_size.width - 1), aligned_vertex.x));
                aligned_vertex.y = std::max(0.0f, std::min(static_cast<float>(image_size.height - 1), aligned_vertex.y));

                aligned_line.push_back(aligned_vertex);
            }

            aligned_lines.push_back(aligned_line);
        }
    }
    
    return aligned_lines;
}

///////////////////////////////////////////////////////////////////////////////

std::string LineAlignmentOperation::getName() const {
    return "Line Alignment to Bright Features";
}

std::type_index LineAlignmentOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<LineData>);
}

bool LineAlignmentOperation::canApply(DataTypeVariant const & dataVariant) const {
    if (!std::holds_alternative<std::shared_ptr<LineData>>(dataVariant)) {
        return false;
    }

    auto const * ptr_ptr = std::get_if<std::shared_ptr<LineData>>(&dataVariant);

    return ptr_ptr && *ptr_ptr;
}

std::unique_ptr<TransformParametersBase> LineAlignmentOperation::getDefaultParameters() const {
    // Note: Returns nullptr since we can't create a GroupingTransformParametersBase
    // without an EntityGroupManager pointer. The calling code will need to provide
    // the actual parameters with the group manager.
    return nullptr;
}

DataTypeVariant LineAlignmentOperation::execute(DataTypeVariant const & dataVariant,
                                                TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, [](int) {});
}

DataTypeVariant LineAlignmentOperation::execute(DataTypeVariant const & dataVariant,
                                                TransformParametersBase const * transformParameters,
                                                ProgressCallback progressCallback) {
    auto const * ptr_ptr = std::get_if<std::shared_ptr<LineData>>(&dataVariant);
    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "LineAlignmentOperation::execute: Incompatible variant type or null data." << std::endl;
        if (progressCallback) progressCallback(100);
        return {};
    }

    auto line_data = *ptr_ptr;

    auto const * typed_params =
            transformParameters ? dynamic_cast<LineAlignmentParameters const *>(transformParameters) : nullptr;

    // Check if we have valid parameters
    if (!typed_params) {
        std::cerr << "LineAlignmentOperation::execute: Invalid parameters provided. Operation requires LineAlignmentParameters." << std::endl;
        if (progressCallback) progressCallback(100);
        return {};
    }

    // Auto-find media data if not provided in parameters
    std::shared_ptr<MediaData> media_data;
    if (typed_params->media_data) {
        media_data = typed_params->media_data;
    } else {
        std::cerr << "LineAlignmentOperation::execute: No media data provided. Operation requires media data to align lines to bright features." << std::endl;
        if (progressCallback) progressCallback(100);
        return {};
    }

    if (progressCallback) progressCallback(0);

    // Use parameters from the typed_params object
    int width = typed_params->width;
    int perpendicular_range = typed_params->perpendicular_range;
    bool use_processed_data = typed_params->use_processed_data;
    FWHMApproach approach = typed_params->approach;
    LineAlignmentOutputMode output_mode = typed_params->output_mode;

    std::shared_ptr<LineData> result = line_alignment(
            line_data.get(),
            media_data.get(),
            width,
            perpendicular_range,
            use_processed_data,
            approach,
            output_mode,
            typed_params,
            progressCallback);

    if (!result) {
        std::cerr << "LineAlignmentOperation::execute: Alignment failed." << std::endl;
        return {};
    }

    std::cout << "LineAlignmentOperation executed successfully." << std::endl;
    return result;
}

std::shared_ptr<LineData> line_alignment(LineData const * line_data,
                                         MediaData * media_data,
                                         int width,
                                         int perpendicular_range,
                                         bool use_processed_data,
                                         FWHMApproach approach,
                                         LineAlignmentOutputMode output_mode,
                                         LineAlignmentParameters const * params) {
    return line_alignment(line_data, media_data, width, perpendicular_range, use_processed_data, approach, output_mode, params, [](int) {});
}

std::shared_ptr<LineData> line_alignment(LineData const * line_data,
                                         MediaData * media_data,
                                         int width,
                                         int perpendicular_range,
                                         bool use_processed_data,
                                         FWHMApproach approach,
                                         LineAlignmentOutputMode output_mode,
                                         LineAlignmentParameters const * params,
                                         ProgressCallback progressCallback) {
    if (!line_data || !media_data) {
        std::cerr << "LineAlignment: Null LineData or MediaData provided." << std::endl;
        if (progressCallback) progressCallback(100);
        return std::make_shared<LineData>();
    }

    // Check if line data has at least 3 vertices
    auto line_times = line_data->getTimesWithData();
    if (line_times.empty()) {
        if (progressCallback) progressCallback(100);
        return std::make_shared<LineData>();
    }

    // Create new LineData for the aligned lines
    auto aligned_line_data = std::make_shared<LineData>();
    aligned_line_data->setImageSize(line_data->getImageSize());

    // Set up grouping if enabled and we have a group manager
    std::map<size_t, std::uint64_t> vertex_to_group_map; // Maps vertex index to group ID
    bool grouping_enabled = params && params->enable_grouping && params->getGroupManager();
    EntityGroupManager* group_manager = grouping_enabled ? params->getGroupManager() : nullptr;

    //aligned_line_data->setIdentityContext(line_data->getIdentityContext());
    
    // Pre-create groups for all vertices if grouping is enabled and we're in FWHM mode
    if (grouping_enabled && params->output_mode == LineAlignmentOutputMode::FWHM_PROFILE_EXTENTS) {
        // Determine the maximum number of vertices by checking all time frames
        size_t max_vertices = 0;
        for (auto time: line_times) {
            auto const & lines = line_data->getAtTime(time);
            for (auto const & line: lines) {
                max_vertices = std::max(max_vertices, line.size());
            }
        }
        
        // Create groups for all possible vertex indices
        for (size_t vertex_idx = 0; vertex_idx < max_vertices; ++vertex_idx) {
            std::string group_name = params->group_prefix + std::to_string(vertex_idx);
            std::string group_desc = params->group_description + " " + std::to_string(vertex_idx);
            std::uint64_t group_id = group_manager->createGroup(group_name, group_desc);
            vertex_to_group_map[vertex_idx] = group_id;
        }
    }

    size_t total_time_points = line_times.size();
    size_t processed_time_points = 0;
    if (progressCallback) progressCallback(0);

    // Process each time that has line data
    for (auto time: line_times) {
        // Get lines at this time
        auto const & lines = line_data->getAtTime(time);
        if (lines.empty()) {
            continue;
        }

        // Get image size
        ImageSize image_size = media_data->getImageSize();

        // Process each line based on the media data type
        std::vector<Line2D> aligned_lines;
        
        // Check if media data is 8-bit or 32-bit and process accordingly
        if (media_data->is8Bit()) {
            // Get 8-bit image data
            std::vector<uint8_t> image_data;
            if (use_processed_data) {
                image_data = media_data->getProcessedData8(static_cast<int>(time.getValue()));
            } else {
                image_data = media_data->getRawData8(static_cast<int>(time.getValue()));
            }

            if (image_data.empty()) {
                continue;
            }

            // Process lines with 8-bit data
            aligned_lines = processLinesWithImageData(lines, image_data, image_size, 
                                                    width, perpendicular_range, approach, output_mode);
        } else {
            // Get 32-bit image data
            std::vector<float> image_data;
            if (use_processed_data) {
                image_data = media_data->getProcessedData32(static_cast<int>(time.getValue()));
            } else {
                image_data = media_data->getRawData32(static_cast<int>(time.getValue()));
            }

            if (image_data.empty()) {
                continue;
            }

            // Process lines with 32-bit data
            aligned_lines = processLinesWithImageData(lines, image_data, image_size, 
                                                    width, perpendicular_range, approach, output_mode);
        }

        // Add the aligned lines to the new LineData
        for (auto const & aligned_line: aligned_lines) {
            aligned_line_data->addAtTime(time, aligned_line, false);
        }
        
        // Handle grouping for FWHM profile extents mode
        if (grouping_enabled && output_mode == LineAlignmentOutputMode::FWHM_PROFILE_EXTENTS) {
            // Get the EntityIds for the lines we just added
            auto const & entity_ids = aligned_line_data->getEntityIdsAtTime(time);
            
            // For FWHM mode, each line corresponds to a vertex from the original line
            for (size_t line_idx = 0; line_idx < entity_ids.size(); ++line_idx) {
                EntityId entity_id = entity_ids[line_idx];
                size_t vertex_index = line_idx; // In FWHM mode, lines are created per vertex
                
                // Add this entity to the appropriate group (groups are already created)
                if (vertex_to_group_map.find(vertex_index) != vertex_to_group_map.end()) {
                    std::uint64_t group_id = vertex_to_group_map[vertex_index];
                    group_manager->addEntityToGroup(group_id, entity_id);
                }
            }
        }

        processed_time_points++;
        if (progressCallback) {
            int current_progress = static_cast<int>(std::round(static_cast<double>(processed_time_points) / static_cast<double>(total_time_points) * 100.0));
            progressCallback(current_progress);
        }
    }

    // Notify group manager of changes if grouping was enabled
    if (grouping_enabled && group_manager) {
        group_manager->notifyGroupsChanged();
    }

    if (progressCallback) progressCallback(100);
    return aligned_line_data;
}

template uint8_t get_pixel_value<uint8_t>(Point2D<float> const & point,
                                          std::vector<uint8_t> const & image_data,
                                          ImageSize const & image_size);

template float get_pixel_value<float>(Point2D<float> const & point,
                                      std::vector<float> const & image_data,
                                      ImageSize const & image_size);


template Point2D<float> calculate_fwhm_center<uint8_t>(Point2D<float> const & vertex,
                                                       Point2D<float> const & perpendicular_dir,
                                                       int width,
                                                       int perpendicular_range,
                                                       std::vector<uint8_t> const & image_data,
                                                       ImageSize const & image_size,
                                                       FWHMApproach approach = FWHMApproach::PEAK_WIDTH_HALF_MAX);
template Point2D<float> calculate_fwhm_center<float>(Point2D<float> const & vertex,
                                                     Point2D<float> const & perpendicular_dir,
                                                     int width,
                                                     int perpendicular_range,
                                                     std::vector<float> const & image_data,
                                                     ImageSize const & image_size,
                                                     FWHMApproach approach = FWHMApproach::PEAK_WIDTH_HALF_MAX);


template Line2D calculate_fwhm_profile_extents<uint8_t>(Point2D<float> const & vertex,
                                                        Point2D<float> const & perpendicular_dir,
                                                        int width,
                                                        int perpendicular_range,
                                                        std::vector<uint8_t> const & image_data,
                                                        ImageSize const & image_size,
                                                        FWHMApproach approach = FWHMApproach::PEAK_WIDTH_HALF_MAX);
template Line2D calculate_fwhm_profile_extents<float>(Point2D<float> const & vertex,
                                                      Point2D<float> const & perpendicular_dir,
                                                      int width,
                                                      int perpendicular_range,
                                                      std::vector<float> const & image_data,
                                                      ImageSize const & image_size,
                                                      FWHMApproach approach = FWHMApproach::PEAK_WIDTH_HALF_MAX);
