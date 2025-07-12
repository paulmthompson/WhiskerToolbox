#include "MaskDataVisualization.hpp"

#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Masks/masks.hpp"

#include <QOpenGLShaderProgram>
#include <QDebug>

#include <algorithm>
#include <limits>

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

    // Create binary image texture
    glGenTextures(1, &binary_image_texture);
    glBindTexture(GL_TEXTURE_2D, binary_image_texture);

    if (!binary_image_data.empty()) {
        auto image_size = mask_data->getImageSize();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, image_size.width, image_size.height, 0,
                     GL_RED, GL_FLOAT, binary_image_data.data());
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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
    if (binary_image_texture != 0) {
        glDeleteTextures(1, &binary_image_texture);
        binary_image_texture = 0;
    }
}

void MaskDataVisualization::updateSelectionOutlineBuffer() {
    /*
    selection_outline_data = generateOutlineDataForMasks(selected_masks);

    selection_outline_array_object.bind();
    selection_outline_buffer.bind();
    
    if (selection_outline_data.empty()) {
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    } else {
        selection_outline_buffer.allocate(selection_outline_data.data(),
                                         static_cast<int>(selection_outline_data.size() * sizeof(float)));
    }

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    selection_outline_buffer.release();
    selection_outline_array_object.release();
    */
}

void MaskDataVisualization::updateHoverOutlineBuffer() {
    /*
    hover_outline_data = generateOutlineDataForMasks(current_hover_masks);

    hover_outline_array_object.bind();
    hover_outline_buffer.bind();
    
    if (hover_outline_data.empty()) {
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    } else {
        hover_outline_buffer.allocate(hover_outline_data.data(),
                                     static_cast<int>(hover_outline_data.size() * sizeof(float)));
    }

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    hover_outline_buffer.release();
    hover_outline_array_object.release();
    */
}

void MaskDataVisualization::clearSelection() {
    if (!selected_masks.empty()) {
        selected_masks.clear();
        updateSelectionOutlineBuffer();
    }
}

bool MaskDataVisualization::toggleMaskSelection(MaskIdentifier const & mask_id) {
    auto it = selected_masks.find(mask_id);

    if (it != selected_masks.end()) {
        // Mask is selected, remove it
        selected_masks.erase(it);
        updateSelectionOutlineBuffer();
        return false; // Mask was deselected
    } else {
        // Mask is not selected, add it
        selected_masks.insert(mask_id);
        updateSelectionOutlineBuffer();
        return true; // Mask was selected
    }
}

void MaskDataVisualization::setHoverMasks(float world_x, float world_y) {
    std::vector<MaskIdentifier> masks = findMasksContainingPoint(world_x, world_y);
    current_hover_masks.clear();
    current_hover_masks.insert(masks.begin(), masks.end());
    updateHoverOutlineBuffer();
}

void MaskDataVisualization::clearHover() {
    if (!current_hover_masks.empty()) {
        current_hover_masks.clear();
        updateHoverOutlineBuffer();
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
        if (maskContainsPoint(candidate.data, pixel_x, pixel_y)) {
            result.push_back(candidate.data);
        }
    }

    qDebug() << "MaskDataVisualization: Found" << result.size() << "masks containing point";
    
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

void MaskDataVisualization::renderMaskOutlines(QOpenGLShaderProgram * shader_program) {
    if (!visible || outline_vertex_data.empty()) return;

    outline_vertex_array_object.bind();
    outline_vertex_buffer.bind();

    // Set color and line width
    QVector4D outline_color = color;
    outline_color.setW(outline_color.w() * 0.8f); // Slightly more transparent
    shader_program->setUniformValue("u_color", outline_color);
    
    glLineWidth(outline_thickness);

    // Draw lines
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(outline_vertex_data.size() / 2));

    outline_vertex_buffer.release();
    outline_vertex_array_object.release();
}

void MaskDataVisualization::renderSelectedMaskOutlines(QOpenGLShaderProgram * shader_program) {
    if (selected_masks.empty() || selection_outline_data.empty()) return;

    selection_outline_array_object.bind();
    selection_outline_buffer.bind();

    // Set uniforms for selection rendering (darker/more opaque)
    QVector4D selection_color = color;
    selection_color.setW(1.0f); // Fully opaque
    selection_color = selection_color * 0.5f; // Darker
    selection_color.setW(1.0f);
    shader_program->setUniformValue("u_color", selection_color);
    
    glLineWidth(outline_thickness * 1.5f); // Thicker lines for selected

    // Draw the selected mask outlines
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(selection_outline_data.size() / 2));

    selection_outline_buffer.release();
    selection_outline_array_object.release();
}

void MaskDataVisualization::renderHoverMaskOutlines(QOpenGLShaderProgram * shader_program) {
    if (current_hover_masks.empty() || hover_outline_data.empty()) return;

    hover_outline_array_object.bind();
    hover_outline_buffer.bind();

    // Set uniforms for hover rendering (bright/highlighted)
    QVector4D hover_color = QVector4D(1.0f, 1.0f, 0.0f, 1.0f); // Bright yellow
    shader_program->setUniformValue("u_color", hover_color);
    
    glLineWidth(outline_thickness * 2.0f); // Thickest lines for hover

    // Draw the hover mask outlines
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(hover_outline_data.size() / 2));

    hover_outline_buffer.release();
    hover_outline_array_object.release();
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

    // Normalize the values to [0, 1] range
    if (!binary_image_data.empty()) {
        float max_value = *std::max_element(binary_image_data.begin(), binary_image_data.end());
        if (max_value > 0.0f) {
            for (auto & value : binary_image_data) {
                value /= max_value;
            }
        }
    }

    qDebug() << "MaskDataVisualization: Binary image texture normalized with" << binary_image_data.size() << "pixels";
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

std::vector<float> MaskDataVisualization::generateOutlineDataForMasks(
    std::unordered_set<MaskIdentifier, MaskIdentifierHash> const & mask_ids) const {
    
    std::vector<float> outline_data;
    
    if (!mask_data) return outline_data;
    
    for (auto const & mask_id : mask_ids) {
        // Find the mask in the data
        bool found = false;
        for (auto const & time_masks_pair : mask_data->getAllAsRange()) {
            if (time_masks_pair.time.getValue() == mask_id.timeframe) {
                if (mask_id.mask_index < time_masks_pair.masks.size()) {
                    auto const & mask = time_masks_pair.masks[mask_id.mask_index];
                    
                    if (!mask.empty()) {
                        // Get outline for this mask
                        auto outline = get_mask_outline(mask);
                        
                        // Convert outline to vertex data (lines)
                        for (size_t i = 0; i < outline.size(); ++i) {
                            size_t next_i = (i + 1) % outline.size();
                            
                            // Add line segment
                            outline_data.push_back(static_cast<float>(outline[i].x));
                            outline_data.push_back(static_cast<float>(outline[i].y));
                            outline_data.push_back(static_cast<float>(outline[next_i].x));
                            outline_data.push_back(static_cast<float>(outline[next_i].y));
                        }
                    }
                    found = true;
                    break;
                }
            }
        }
        
        if (!found) {
            qDebug() << "MaskDataVisualization: Could not find mask" << mask_id.timeframe << mask_id.mask_index;
        }
    }
    
    return outline_data;
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
        } else {
            return false;
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
