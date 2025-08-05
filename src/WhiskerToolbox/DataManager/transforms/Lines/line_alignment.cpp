#include "line_alignment.hpp"

#include "Lines/Line_Data.hpp"
#include "Media/Media_Data.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <vector>

// Calculate perpendicular direction at a line vertex
Point2D<float> calculate_perpendicular_direction(Line2D const & line, size_t vertex_index) {
    if (line.size() < 3) {
        // For lines with fewer than 3 points, we can't calculate a meaningful perpendicular
        return Point2D<float>{0.0f, 0.0f};
    }

    if (vertex_index == 0) {
        // First vertex: use the direction from vertex 0 to vertex 1
        Point2D<float> dir{line[1].x - line[0].x, line[1].y - line[0].y};
        float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (length > 0) {
            return Point2D<float>{-dir.y / length, dir.x / length}; // Perpendicular
        }
    } else if (vertex_index == line.size() - 1) {
        // Last vertex: use the direction from vertex n-2 to vertex n-1
        Point2D<float> dir{line[line.size() - 1].x - line[line.size() - 2].x, 
                           line[line.size() - 1].y - line[line.size() - 2].y};
        float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (length > 0) {
            return Point2D<float>{-dir.y / length, dir.x / length}; // Perpendicular
        }
    } else {
        // Middle vertices: average the perpendicular directions from adjacent segments
        Point2D<float> dir1{line[vertex_index].x - line[vertex_index - 1].x, 
                            line[vertex_index].y - line[vertex_index - 1].y};
        Point2D<float> dir2{line[vertex_index + 1].x - line[vertex_index].x, 
                            line[vertex_index + 1].y - line[vertex_index].y};
        
        float length1 = std::sqrt(dir1.x * dir1.x + dir1.y * dir1.y);
        float length2 = std::sqrt(dir2.x * dir2.x + dir2.y * dir2.y);
        
        if (length1 > 0 && length2 > 0) {
            Point2D<float> perp1{-dir1.y / length1, dir1.x / length1};
            Point2D<float> perp2{-dir2.y / length2, dir2.x / length2};
            
            // Average the perpendicular directions
            Point2D<float> avg_perp{(perp1.x + perp2.x) / 2.0f, (perp1.y + perp2.y) / 2.0f};
            float avg_length = std::sqrt(avg_perp.x * avg_perp.x + avg_perp.y * avg_perp.y);
            
            if (avg_length > 0) {
                return Point2D<float>{avg_perp.x / avg_length, avg_perp.y / avg_length};
            }
        }
    }
    
    return Point2D<float>{0.0f, 0.0f};
}

// Helper function to get pixel value at a given position
uint8_t get_pixel_value(Point2D<float> const & point, 
                        std::vector<uint8_t> const & image_data, 
                        ImageSize const & image_size) {
    int x = static_cast<int>(std::round(point.x));
    int y = static_cast<int>(std::round(point.y));
    
    // Check bounds
    if (x < 0 || x >= image_size.width || y < 0 || y >= image_size.height) {
        return 0;
    }
    
    size_t index = static_cast<size_t>(y * image_size.width + x);
    if (index < image_data.size()) {
        return image_data[index];
    }
    
    return 0;
}

// Calculate FWHM displacement for a single vertex
float calculate_fwhm_displacement(Point2D<float> const & vertex,
                                 Point2D<float> const & perpendicular_dir,
                                 int width,
                                 std::vector<uint8_t> const & image_data,
                                 ImageSize const & image_size,
                                 FWHMApproach approach) {
    if (width <= 0) {
        return 0.0f;
    }
    
    std::vector<float> displacements;
    std::vector<float> intensities;
    
    // Sample multiple points along the width of the analysis strip
    for (int w = -width/2; w <= width/2; ++w) {
        // Calculate the sampling point along the width direction
        Point2D<float> width_dir{-perpendicular_dir.y, perpendicular_dir.x}; // Perpendicular to perp dir
        
        Point2D<float> sample_start = {
            vertex.x + width_dir.x * w,
            vertex.y + width_dir.y * w
        };
        
        // Sample intensity profile along the perpendicular direction
        std::vector<uint8_t> profile;
        std::vector<float> distances;
        
        // Sample up to 50 pixels in each direction along the perpendicular
        for (int d = -25; d <= 25; ++d) {
            Point2D<float> sample_point = {
                sample_start.x + perpendicular_dir.x * d,
                sample_start.y + perpendicular_dir.y * d
            };
            
            uint8_t intensity = get_pixel_value(sample_point, image_data, image_size);
            profile.push_back(intensity);
            distances.push_back(static_cast<float>(d));
        }
        
        // Find the maximum intensity in the profile
        auto max_it = std::max_element(profile.begin(), profile.end());
        if (max_it == profile.end()) {
            continue;
        }
        
        uint8_t max_intensity = *max_it;
        if (max_intensity == 0) {
            continue; // Skip if no signal
        }
        
        // Find the half-maximum threshold
        uint8_t half_max = max_intensity / 2;
        
        // Find the left and right boundaries at half maximum
        int left_bound = -25;
        int right_bound = 25;
        
        for (int i = 0; i < static_cast<int>(profile.size()); ++i) {
            if (profile[i] >= half_max) {
                left_bound = i - 25;
                break;
            }
        }
        
        for (int i = static_cast<int>(profile.size()) - 1; i >= 0; --i) {
            if (profile[i] >= half_max) {
                right_bound = i - 25;
                break;
            }
        }
        
        // Calculate the center of the FWHM region
        float center_offset = (left_bound + right_bound) / 2.0f;
        displacements.push_back(center_offset);
        intensities.push_back(static_cast<float>(max_intensity));
    }
    
    // Calculate weighted average displacement based on intensity
    if (displacements.empty()) {
        return 0.0f;
    }
    
    float total_weight = 0.0f;
    float weighted_sum = 0.0f;
    
    for (size_t i = 0; i < displacements.size(); ++i) {
        float weight = intensities[i];
        weighted_sum += displacements[i] * weight;
        total_weight += weight;
    }
    
    if (total_weight > 0) {
        return weighted_sum / total_weight;
    }
    
    return 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

std::string LineAlignmentOperation::getName() const {
    return "Line Alignment to Bright Objects";
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
    return std::make_unique<LineAlignmentParameters>();
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

    if (!typed_params || !typed_params->media_data) {
        std::cerr << "LineAlignmentOperation::execute: Missing media data in parameters." << std::endl;
        if (progressCallback) progressCallback(100);
        return {};
    }

    if (progressCallback) progressCallback(0);
    std::shared_ptr<LineData> result = line_alignment(
            line_data.get(),
            typed_params->media_data.get(),
            typed_params->width,
            typed_params->use_processed_data,
            typed_params->approach,
            progressCallback
    );

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
                                         bool use_processed_data,
                                         FWHMApproach approach) {
    return line_alignment(line_data, media_data, width, use_processed_data, approach, [](int) {});
}

std::shared_ptr<LineData> line_alignment(LineData const * line_data,
                                         MediaData * media_data,
                                         int width,
                                         bool use_processed_data,
                                         FWHMApproach approach,
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

    size_t total_time_points = line_times.size();
    size_t processed_time_points = 0;
    if (progressCallback) progressCallback(0);

    // Process each time that has line data
    for (auto time : line_times) {
        // Get lines at this time
        auto const & lines = line_data->getAtTime(time);
        if (lines.empty()) {
            continue;
        }

        // Get media data for this time
        std::vector<uint8_t> image_data;
        if (use_processed_data) {
            image_data = media_data->getProcessedData(time.getValue());
        } else {
            image_data = media_data->getRawData(time.getValue());
        }

        if (image_data.empty()) {
            continue;
        }

        ImageSize image_size = media_data->getImageSize();

        // Process each line
        std::vector<Line2D> aligned_lines;
        for (auto const & line : lines) {
            if (line.size() < 3) {
                // Skip lines with fewer than 3 vertices
                aligned_lines.push_back(line);
                continue;
            }

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
                
                // Calculate FWHM displacement
                float displacement = calculate_fwhm_displacement(
                    vertex, perp_dir, width, image_data, image_size, approach);
                
                // Apply the displacement
                Point2D<float> aligned_vertex = {
                    vertex.x + perp_dir.x * displacement,
                    vertex.y + perp_dir.y * displacement
                };
                
                // Ensure the aligned vertex stays within image bounds
                aligned_vertex.x = std::max(0.0f, std::min(static_cast<float>(image_size.width - 1), aligned_vertex.x));
                aligned_vertex.y = std::max(0.0f, std::min(static_cast<float>(image_size.height - 1), aligned_vertex.y));
                
                aligned_line.push_back(aligned_vertex);
            }
            
            aligned_lines.push_back(aligned_line);
        }

        // Add the aligned lines to the new LineData
        for (auto const & aligned_line : aligned_lines) {
            aligned_line_data->addAtTime(time, aligned_line, false);
        }

        processed_time_points++;
        if (progressCallback) {
            int current_progress = static_cast<int>(std::round(static_cast<double>(processed_time_points) / total_time_points * 100.0));
            progressCallback(current_progress);
        }
    }

    if (progressCallback) progressCallback(100);
    return aligned_line_data;
} 