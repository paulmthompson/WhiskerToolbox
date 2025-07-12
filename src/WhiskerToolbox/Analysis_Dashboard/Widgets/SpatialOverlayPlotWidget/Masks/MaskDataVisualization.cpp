#include "MaskDataVisualization.hpp"

#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Masks/masks.hpp"

#include <QOpenGLShaderProgram>
#include <QDebug>

#include <algorithm>
#include <limits>
#include <cmath>

MaskDataVisualization::MaskDataVisualization(QString const & data_key,
                                             std::shared_ptr<MaskData> const & mask_data)
    : key(data_key),
      mask_data(mask_data),
      quad_vertex_buffer(QOpenGLBuffer::VertexBuffer),
      outline_vertex_buffer(QOpenGLBuffer::VertexBuffer),
      selection_outline_buffer(QOpenGLBuffer::VertexBuffer),
      hover_outline_buffer(QOpenGLBuffer::VertexBuffer),
      color(1.0f, 0.0f, 0.0f, 1.0f) {
      
    if (!mask_data) {
        qDebug() << "MaskDataVisualization: Null mask data provided";
        return;
    }
    
    // Set world bounds based on image size
    auto image_size = mask_data->getImageSize();
    world_min_x = 0.0f;
    world_max_x = static_cast<float>(image_size.width);
    world_min_y = 0.0f;
    world_max_y = static_cast<float>(image_size.height);
    
    spatial_index = std::make_unique<RTree<MaskIdentifier>>();
    
    // Precompute all visualization data
    populateRTree();
    createBinaryImageTexture();
    generateOutlineVertexData();
    
    initializeOpenGLResources();
}

MaskDataVisualization::~MaskDataVisualization() {
    cleanupOpenGLResources();
}

void MaskDataVisualization::initializeOpenGLResources() {
    if (!initializeOpenGLFunctions()) {
        qDebug() << "MaskDataVisualization: Failed to initialize OpenGL functions";
        return;
    }

    // Create quad vertex buffer for texture rendering
    quad_vertex_array_object.create();
    quad_vertex_array_object.bind();

    quad_vertex_buffer.create();
    quad_vertex_buffer.bind();
    quad_vertex_buffer.setUsagePattern(QOpenGLBuffer::StaticDraw);

    // Quad vertices covering the world bounds
    float quad_vertices[] = {
        world_min_x, world_min_y, 0.0f, 0.0f,  // Bottom-left
        world_max_x, world_min_y, 1.0f, 0.0f,  // Bottom-right
        world_max_x, world_max_y, 1.0f, 1.0f,  // Top-right
        world_min_x, world_max_y, 0.0f, 1.0f   // Top-left
    };

    quad_vertex_buffer.allocate(quad_vertices, sizeof(quad_vertices));

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    
    // Texture coordinate attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 
                         reinterpret_cast<void*>(2 * sizeof(float)));

    quad_vertex_buffer.release();
    quad_vertex_array_object.release();

    // Create outline vertex buffer
    outline_vertex_array_object.create();
    outline_vertex_array_object.bind();

    outline_vertex_buffer.create();
    outline_vertex_buffer.bind();
    outline_vertex_buffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    
    if (!outline_vertex_data.empty()) {
        qDebug() << "MaskDataVisualization: Allocating outline vertex data with" << outline_vertex_data.size() << "vertices";
        outline_vertex_buffer.allocate(outline_vertex_data.data(), 
                                      static_cast<int>(outline_vertex_data.size() * sizeof(float)));
    }

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    outline_vertex_buffer.release();
    outline_vertex_array_object.release();

    // Create selection outline buffer
    selection_outline_array_object.create();
    selection_outline_array_object.bind();

    selection_outline_buffer.create();
    selection_outline_buffer.bind();
    selection_outline_buffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    selection_outline_buffer.release();
    selection_outline_array_object.release();

    // Create hover outline buffer
    hover_outline_array_object.create();
    hover_outline_array_object.bind();

    hover_outline_buffer.create();
    hover_outline_buffer.bind();
    hover_outline_buffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    hover_outline_buffer.release();
    hover_outline_array_object.release();

    // Create hover bounding box buffer
    hover_bbox_array_object.create();
    hover_bbox_array_object.bind();

    hover_bbox_buffer.create();
    hover_bbox_buffer.bind();
    hover_bbox_buffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    hover_bbox_buffer.release();
    hover_bbox_array_object.release();

    // Create binary image texture
    glGenTextures(1, &binary_image_texture);
    glBindTexture(GL_TEXTURE_2D, binary_image_texture);

    if (!binary_image_data.empty()) {
        auto image_size = mask_data->getImageSize();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, image_size.width, image_size.height, 0,
                     GL_RED, GL_FLOAT, binary_image_data.data());
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void MaskDataVisualization::cleanupOpenGLResources() {
    if (quad_vertex_buffer.isCreated()) {
        quad_vertex_buffer.destroy();
    }
    if (quad_vertex_array_object.isCreated()) {
        quad_vertex_array_object.destroy();
    }
    if (outline_vertex_buffer.isCreated()) {
        outline_vertex_buffer.destroy();
    }
    if (outline_vertex_array_object.isCreated()) {
        outline_vertex_array_object.destroy();
    }
    if (selection_outline_buffer.isCreated()) {
        selection_outline_buffer.destroy();
    }
    if (selection_outline_array_object.isCreated()) {
        selection_outline_array_object.destroy();
    }
    if (hover_outline_buffer.isCreated()) {
        hover_outline_buffer.destroy();
    }
    if (hover_outline_array_object.isCreated()) {
        hover_outline_array_object.destroy();
    }
    if (hover_bbox_buffer.isCreated()) {
        hover_bbox_buffer.destroy();
    }
    if (hover_bbox_array_object.isCreated()) {
        hover_bbox_array_object.destroy();
    }
    if (binary_image_texture != 0) {
        glDeleteTextures(1, &binary_image_texture);
        binary_image_texture = 0;
    }
    if (selection_binary_image_texture != 0) {
        glDeleteTextures(1, &selection_binary_image_texture);
        selection_binary_image_texture = 0;
    }
}

void MaskDataVisualization::clearSelection() {
    if (!selected_masks.empty()) {
        selected_masks.clear();
        updateSelectionBinaryImageTexture();
    }
}

void MaskDataVisualization::selectMasks(std::vector<MaskIdentifier> const & mask_ids) {
    qDebug() << "MaskDataVisualization: Selecting" << mask_ids.size() << "masks";
    
    for (auto const & mask_id : mask_ids) {
        selected_masks.insert(mask_id);
    }
    
    updateSelectionBinaryImageTexture();
    qDebug() << "MaskDataVisualization: Total selected masks:" << selected_masks.size();
}

bool MaskDataVisualization::toggleMaskSelection(MaskIdentifier const & mask_id) {
    auto it = selected_masks.find(mask_id);
    
    if (it != selected_masks.end()) {
        // Mask is selected, remove it
        selected_masks.erase(it);
        updateSelectionBinaryImageTexture();
        qDebug() << "MaskDataVisualization: Deselected mask" << mask_id.timeframe << "," << mask_id.mask_index 
                 << "- Total selected:" << selected_masks.size();
        return false; // Mask was deselected
    } else {
        // Mask is not selected, add it
        selected_masks.insert(mask_id);
        updateSelectionBinaryImageTexture();
        qDebug() << "MaskDataVisualization: Selected mask" << mask_id.timeframe << "," << mask_id.mask_index
                 << "- Total selected:" << selected_masks.size();
        return true; // Mask was selected
    }
}

void MaskDataVisualization::setHoverEntries(std::vector<RTreeEntry<MaskIdentifier>> const & entries) {
    current_hover_entries = entries;
    updateHoverBoundingBoxBuffer();
}

void MaskDataVisualization::clearHover() {
    if (!current_hover_entries.empty()) {
        current_hover_entries.clear();
        updateHoverBoundingBoxBuffer();
    }
}

std::vector<MaskIdentifier> MaskDataVisualization::findMasksContainingPoint(float world_x, float world_y) const {
    std::vector<MaskIdentifier> result;
    
    if (!spatial_index) return result;
    
    // Use R-tree to find candidate masks
    qDebug() << "MaskDataVisualization: Finding masks containing point" << world_x << world_y;
    BoundingBox point_bbox(world_x, world_y, world_x, world_y);
    std::vector<RTreeEntry<MaskIdentifier>> candidates;
    spatial_index->query(point_bbox, candidates);

    qDebug() << "MaskDataVisualization: Found" << candidates.size() << "candidates from R-tree";
    
    // Check each candidate mask for actual point containment
    uint32_t pixel_x = static_cast<uint32_t>(std::round(world_x));
    uint32_t pixel_y = static_cast<uint32_t>(std::round(world_y));

    for (auto const & candidate : candidates) {
        /*
        if (maskContainsPoint(candidate.data, pixel_x, pixel_y)) {
            result.push_back(candidate.data);
        }
        */
       result.push_back(candidate.data); // Use faster RTreeEntry data directly for now
    }

    qDebug() << "MaskDataVisualization: Found" << result.size() << "masks containing point";
    
    return result;
}

std::vector<MaskIdentifier> MaskDataVisualization::refineMasksContainingPoint(std::vector<RTreeEntry<MaskIdentifier>> const & entries, float world_x, float world_y) const {
    std::vector<MaskIdentifier> result;
    
    if (!mask_data) return result;
    
    qDebug() << "MaskDataVisualization: Refining" << entries.size() << "R-tree entries using precise point checking";
    
    // Check each candidate mask for actual point containment
    uint32_t pixel_x = static_cast<uint32_t>(std::round(world_x));
    uint32_t pixel_y = static_cast<uint32_t>(std::round(world_y));

    for (auto const & entry : entries) {
        if (maskContainsPoint(entry.data, pixel_x, pixel_y)) {
            result.push_back(entry.data);
        }
    }

    qDebug() << "MaskDataVisualization: Refined to" << result.size() << "masks containing point after precise checking";
    
    return result;
}

void MaskDataVisualization::renderBinaryImage(QOpenGLShaderProgram * shader_program) {
    if (!visible || binary_image_texture == 0) return;

    quad_vertex_array_object.bind();
    quad_vertex_buffer.bind();

    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, binary_image_texture);
    shader_program->setUniformValue("u_texture", 0);
    shader_program->setUniformValue("u_color", color);

    // Draw quad
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glBindTexture(GL_TEXTURE_2D, 0);
    quad_vertex_buffer.release();
    quad_vertex_array_object.release();
}

BoundingBox MaskDataVisualization::calculateBounds() const {
    if (!mask_data) {
        return BoundingBox(0, 0, 0, 0);
    }

    auto image_size = mask_data->getImageSize();
    return BoundingBox(0, 0, static_cast<float>(image_size.width), static_cast<float>(image_size.height));
}

void MaskDataVisualization::setOutlineThickness(float thickness) {
    outline_thickness = std::max(0.5f, thickness);
}

void MaskDataVisualization::createBinaryImageTexture() {
    if (!mask_data) return;

    qDebug() << "MaskDataVisualization: Creating binary image texture with" << mask_data->size() << "time frames";

    auto image_size = mask_data->getImageSize();
    binary_image_data.resize(image_size.width * image_size.height, 0.0f);

    // Aggregate all masks into the binary image
    for (auto const & time_masks_pair : mask_data->getAllAsRange()) {
        for (auto const & mask : time_masks_pair.masks) {
            for (auto const & point : mask) {
                if (point.x < image_size.width && point.y < image_size.height) {
                    size_t index = point.y * image_size.width + point.x;
                    binary_image_data[index] += 1.0f;
                }
            }
        }
    }

    qDebug() << "MaskDataVisualization: Binary image texture created with" << binary_image_data.size() << "pixels";

    // Apply non-linear scaling to improve visibility of sparse regions
    if (!binary_image_data.empty()) {
        float max_value = *std::max_element(binary_image_data.begin(), binary_image_data.end());
        qDebug() << "MaskDataVisualization: Max mask density:" << max_value;
        
        if (max_value > 0.0f) {
            // Use logarithmic scaling to compress the dynamic range
            // This makes sparse areas more visible while preserving dense areas
            for (auto & value : binary_image_data) {
                if (value > 0.0f) {
                    // Apply log scaling: log(1 + value) / log(1 + max_value)
                    // This ensures that even single masks (value=1) are visible
                    value = std::log(1.0f + value) / std::log(1.0f + max_value);
                }
            }
            
            // Debug: Check scaled values
            float min_scaled = *std::min_element(binary_image_data.begin(), binary_image_data.end());
            float max_scaled = *std::max_element(binary_image_data.begin(), binary_image_data.end());
            qDebug() << "MaskDataVisualization: Scaled texture range: min=" << min_scaled << "max=" << max_scaled;
        }
    }

    qDebug() << "MaskDataVisualization: Binary image texture scaled with logarithmic normalization";
}

void MaskDataVisualization::updateSelectionBinaryImageTexture() {
    if (!mask_data) return;

    qDebug() << "MaskDataVisualization: Updating selection binary image texture with" << selected_masks.size() << "selected masks";

    auto image_size = mask_data->getImageSize();
    selection_binary_image_data.clear();
    selection_binary_image_data.resize(image_size.width * image_size.height, 0.0f);

    // Only include selected masks in the selection binary image
    for (auto const & mask_id : selected_masks) {
        auto const & masks = mask_data->getAtTime(TimeFrameIndex(mask_id.timeframe));
        if (mask_id.mask_index < masks.size()) {
            auto const & mask = masks[mask_id.mask_index];
            
            for (auto const & point : mask) {
                if (point.x < image_size.width && point.y < image_size.height) {
                    size_t index = point.y * image_size.width + point.x;
                    selection_binary_image_data[index] = 1.0f; // Uniform opacity for selected masks
                }
            }
        }
    }

    // Update the OpenGL texture if it exists
    if (selection_binary_image_texture != 0) {
        glBindTexture(GL_TEXTURE_2D, selection_binary_image_texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image_size.width, image_size.height,
                        GL_RED, GL_FLOAT, selection_binary_image_data.data());
        glBindTexture(GL_TEXTURE_2D, 0);
    } else {
        // Create the texture if it doesn't exist
        glGenTextures(1, &selection_binary_image_texture);
        glBindTexture(GL_TEXTURE_2D, selection_binary_image_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, image_size.width, image_size.height, 0,
                     GL_RED, GL_FLOAT, selection_binary_image_data.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    qDebug() << "MaskDataVisualization: Selection binary image texture updated";
}

void MaskDataVisualization::populateRTree() {
    if (!mask_data || !spatial_index) return;

    qDebug() << "MaskDataVisualization: Populating R-tree with" << mask_data->size() << "time frames";

    for (auto const & time_masks_pair : mask_data->getAllAsRange()) {
        for (size_t mask_index = 0; mask_index < time_masks_pair.masks.size(); ++mask_index) {
            auto const & mask = time_masks_pair.masks[mask_index];
            
            if (mask.empty()) continue;

            // Calculate bounding box for this mask
            auto [min_point, max_point] = get_bounding_box(mask);
            
            BoundingBox bbox(static_cast<float>(min_point.x), static_cast<float>(min_point.y),
                           static_cast<float>(max_point.x), static_cast<float>(max_point.y));
            
            MaskIdentifier mask_id(time_masks_pair.time.getValue(), mask_index);
            spatial_index->insert(bbox, mask_id);

        }
    }

    qDebug() << "MaskDataVisualization: R-tree populated with" << spatial_index->size() << "masks";
}

void MaskDataVisualization::generateOutlineVertexData() {
    if (!mask_data) return;

    outline_vertex_data.clear();

    qDebug() << "MaskDataVisualization: Generating outline vertex data with" << mask_data->size() << "time frames";

    /*
    for (auto const & time_masks_pair : mask_data->getAllAsRange()) {
        for (size_t mask_index = 0; mask_index < time_masks_pair.masks.size(); ++mask_index) {
            auto const & mask = time_masks_pair.masks[mask_index];
            
            if (mask.empty()) continue;

            // Get outline for this mask
            auto outline = get_mask_outline(mask);
            
            // Convert outline to vertex data (lines)
            for (size_t i = 0; i < outline.size(); ++i) {
                size_t next_i = (i + 1) % outline.size();
                
                // Add line segment
                outline_vertex_data.push_back(static_cast<float>(outline[i].x));
                outline_vertex_data.push_back(static_cast<float>(outline[i].y));
                outline_vertex_data.push_back(static_cast<float>(outline[next_i].x));
                outline_vertex_data.push_back(static_cast<float>(outline[next_i].y));
            }

            qDebug() << "MaskDataVisualization: Outline vertex data generated for mask" 
                     << time_masks_pair.time.getValue() << "with" 
                     << outline_vertex_data.size() << "vertices";
        }
    }
    */
    qDebug() << "MaskDataVisualization: Outline vertex data generated with" << outline_vertex_data.size() << "vertices";
}

bool MaskDataVisualization::maskContainsPoint(MaskIdentifier const & mask_id, uint32_t pixel_x, uint32_t pixel_y) const {
    if (!mask_data) return false;

    //return true;
    
    auto const & masks = mask_data->getAtTime(TimeFrameIndex(mask_id.timeframe));

    if (masks.empty()) return false;

    auto const & mask = masks[mask_id.mask_index];

    for (auto const & point : mask) {
        if (point.x == pixel_x && point.y == pixel_y) {
            return true;
        } 
    }

    return false;
}

std::pair<float, float> MaskDataVisualization::worldToTexture(float world_x, float world_y) const {
    if (!mask_data) return {0.0f, 0.0f};
    
    auto image_size = mask_data->getImageSize();
    
    float u = (world_x - world_min_x) / (world_max_x - world_min_x);
    float v = (world_y - world_min_y) / (world_max_y - world_min_y);
    
    return {u, v};
}

void MaskDataVisualization::renderHoverMaskBoundingBoxes(QOpenGLShaderProgram * shader_program) {
    if (current_hover_entries.empty() || hover_bbox_data.empty()) return;

    hover_bbox_array_object.bind();
    hover_bbox_buffer.bind();

    // Set uniforms for bounding box rendering (black)
    QVector4D bbox_color = QVector4D(0.0f, 0.0f, 0.0f, 1.0f); // Black
    shader_program->setUniformValue("u_color", bbox_color);
    
    glLineWidth(2.0f); // Thick lines for visibility

    // Draw the bounding boxes
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(hover_bbox_data.size() / 2));

    hover_bbox_buffer.release();
    hover_bbox_array_object.release();
}

void MaskDataVisualization::renderSelectedMasks(QOpenGLShaderProgram * shader_program) {
    if (!visible || selection_binary_image_texture == 0 || selected_masks.empty()) return;

    quad_vertex_array_object.bind();
    quad_vertex_buffer.bind();

    // Bind selection texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, selection_binary_image_texture);
    shader_program->setUniformValue("u_texture", 0);
    
    // Set different color for selected masks (e.g., yellow with some transparency)
    QVector4D selection_color = QVector4D(1.0f, 1.0f, 0.0f, 0.7f); // Yellow with 70% opacity
    shader_program->setUniformValue("u_color", selection_color);

    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Draw quad
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // Disable blending
    glDisable(GL_BLEND);

    glBindTexture(GL_TEXTURE_2D, 0);
    quad_vertex_buffer.release();
    quad_vertex_array_object.release();
}

void MaskDataVisualization::updateHoverBoundingBoxBuffer() {
    hover_bbox_data = generateBoundingBoxDataFromEntries(current_hover_entries);

    hover_bbox_array_object.bind();
    hover_bbox_buffer.bind();
    
    if (hover_bbox_data.empty()) {
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    } else {
        hover_bbox_buffer.allocate(hover_bbox_data.data(),
                                  static_cast<int>(hover_bbox_data.size() * sizeof(float)));
    }

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    hover_bbox_buffer.release();
    hover_bbox_array_object.release();
}

std::vector<float> MaskDataVisualization::generateBoundingBoxDataFromEntries(
    std::vector<RTreeEntry<MaskIdentifier>> const & entries) const {
    
    std::vector<float> bbox_data;
    
    for (auto const & entry : entries) {
        // Use the bounding box data directly from the R-tree entry
        float min_x = entry.min_x;
        float min_y = entry.min_y;
        float max_x = entry.max_x;
        float max_y = entry.max_y;

        // Generate line segments for the bounding box rectangle
        // Bottom edge
        bbox_data.push_back(min_x); bbox_data.push_back(min_y);
        bbox_data.push_back(max_x); bbox_data.push_back(min_y);
        
        // Right edge
        bbox_data.push_back(max_x); bbox_data.push_back(min_y);
        bbox_data.push_back(max_x); bbox_data.push_back(max_y);
        
        // Top edge
        bbox_data.push_back(max_x); bbox_data.push_back(max_y);
        bbox_data.push_back(min_x); bbox_data.push_back(max_y);
        
        // Left edge
        bbox_data.push_back(min_x); bbox_data.push_back(max_y);
        bbox_data.push_back(min_x); bbox_data.push_back(min_y);
    }
    
    return bbox_data;
}
