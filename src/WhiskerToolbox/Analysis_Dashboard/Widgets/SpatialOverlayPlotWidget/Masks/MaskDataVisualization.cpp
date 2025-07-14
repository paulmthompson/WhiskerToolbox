#include "MaskDataVisualization.hpp"

#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/polygon_adapter.hpp"
#include "DataManager/Masks/Mask_Data.hpp"

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
      color(1.0f, 0.0f, 0.0f, 1.0f),
      hover_union_polygon(std::vector<Point2D<float>>()) {
      
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


    // Create hover union polygon buffer
    hover_polygon_array_object.create();
    hover_polygon_array_object.bind();

    hover_polygon_buffer.create();
    hover_polygon_buffer.bind();
    hover_polygon_buffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    hover_polygon_buffer.release();
    hover_polygon_array_object.release();

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

    if (hover_polygon_buffer.isCreated()) {
        hover_polygon_buffer.destroy();
    }
    if (hover_polygon_array_object.isCreated()) {
        hover_polygon_array_object.destroy();
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

bool MaskDataVisualization::removeMaskFromSelection(MaskIdentifier const & mask_id) {
    auto it = selected_masks.find(mask_id);
    
    if (it != selected_masks.end()) {
        // Mask is selected, remove it
        selected_masks.erase(it);
        updateSelectionBinaryImageTexture();
        qDebug() << "MaskDataVisualization: Removed mask" << mask_id.timeframe << "," << mask_id.mask_index
                 << "from selection - Total selected:" << selected_masks.size();
        return true; // Mask was removed
    }
    
    return false; // Mask wasn't selected
}

size_t MaskDataVisualization::removeIntersectingMasks(std::vector<MaskIdentifier> const & mask_ids) {
    size_t removed_count = 0;
    
    // Find intersection between current selection and provided mask_ids
    for (auto const & mask_id : mask_ids) {
        auto it = selected_masks.find(mask_id);
        if (it != selected_masks.end()) {
            // This mask is in both sets - remove it from selection
            selected_masks.erase(it);
            removed_count++;
            qDebug() << "MaskDataVisualization: Removed intersecting mask" 
                     << mask_id.timeframe << "," << mask_id.mask_index;
        }
    }
    
    if (removed_count > 0) {
        updateSelectionBinaryImageTexture();
        qDebug() << "MaskDataVisualization: Removed" << removed_count 
                 << "intersecting masks - Total selected:" << selected_masks.size();
    }
    
    return removed_count;
}

void MaskDataVisualization::setHoverEntries(std::vector<RTreeEntry<MaskIdentifier>> const & entries) {
    current_hover_entries = entries;
    updateHoverUnionPolygon();
}

void MaskDataVisualization::clearHover() {
    if (!current_hover_entries.empty()) {
        current_hover_entries.clear();
        updateHoverUnionPolygon();
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

void MaskDataVisualization::renderHoverMaskUnionPolygon(QOpenGLShaderProgram * shader_program) {
    if (current_hover_entries.empty() || hover_polygon_data.empty()) return;

    hover_polygon_array_object.bind();
    hover_polygon_buffer.bind();

    // Set uniforms for polygon rendering (black outline)
    QVector4D polygon_color = QVector4D(0.0f, 0.0f, 0.0f, 1.0f); // Black
    shader_program->setUniformValue("u_color", polygon_color);
    
    glLineWidth(3.0f); // Thick lines for visibility

    // Draw the union polygon as a line loop
    glDrawArrays(GL_LINE_LOOP, 0, static_cast<GLsizei>(hover_polygon_data.size() / 2));

    hover_polygon_buffer.release();
    hover_polygon_array_object.release();
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

void MaskDataVisualization::updateHoverUnionPolygon() {
    if (current_hover_entries.empty()) {
        hover_union_polygon = Polygon(std::vector<Point2D<float>>{});
        hover_polygon_data.clear();
    } else {
        hover_union_polygon = computeUnionPolygonFromEntries(current_hover_entries);
        hover_polygon_data = generatePolygonVertexData(hover_union_polygon);
    }

    hover_polygon_array_object.bind();
    hover_polygon_buffer.bind();
    
    if (hover_polygon_data.empty()) {
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    } else {
        hover_polygon_buffer.allocate(hover_polygon_data.data(),
                                    static_cast<int>(hover_polygon_data.size() * sizeof(float)));
    }

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    hover_polygon_buffer.release();
    hover_polygon_array_object.release();
}

Polygon MaskDataVisualization::computeUnionPolygonFromEntries(std::vector<RTreeEntry<MaskIdentifier>> const & entries) const {
    if (entries.empty()) {
        return Polygon(std::vector<Point2D<float>>{});
    }
    
    if (entries.size() == 1) {
        // Single bounding box - just convert to polygon
        BoundingBox bbox(entries[0].min_x, entries[0].min_y, entries[0].max_x, entries[0].max_y);
        return Polygon(bbox);
    }
    
    qDebug() << "MaskDataVisualization: Computing efficient union of" << entries.size() << "bounding boxes";
    
    // Convert entries to BoundingBox objects with their areas
    std::vector<std::pair<BoundingBox, float>> bbox_with_areas;
    bbox_with_areas.reserve(entries.size());
    
    for (const auto& entry : entries) {
        BoundingBox bbox(entry.min_x, entry.min_y, entry.max_x, entry.max_y);
        float area = bbox.width() * bbox.height();
        bbox_with_areas.emplace_back(bbox, area);
    }
    
    // Sort by area (largest first) for optimal processing
    std::sort(bbox_with_areas.begin(), bbox_with_areas.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Start with the largest bounding box
    std::vector<BoundingBox> essential_boxes;
    essential_boxes.push_back(bbox_with_areas[0].first);
    
    qDebug() << "MaskDataVisualization: Starting with largest box, area:" << bbox_with_areas[0].second;
    
    // Process remaining boxes, eliminating those that are completely contained
    for (size_t i = 1; i < bbox_with_areas.size(); ++i) {
        const BoundingBox& candidate = bbox_with_areas[i].first;
        
        bool is_contained = false;
        // Check if this box is completely contained within any existing box
        for (const auto& existing : essential_boxes) {
            if (isCompletelyContained(candidate, existing)) {
                is_contained = true;
                break;
            }
        }
        
        if (!is_contained) {
            // Check if this box completely contains any existing boxes (and remove them)
            essential_boxes.erase(
                std::remove_if(essential_boxes.begin(), essential_boxes.end(),
                              [&candidate](const BoundingBox& existing) {
                                  return isCompletelyContained(existing, candidate);
                              }),
                essential_boxes.end());
            
            essential_boxes.push_back(candidate);
        }
    }
    
    qDebug() << "MaskDataVisualization: Reduced from" << entries.size() << "to" << essential_boxes.size() << "essential boxes";
    
    if (essential_boxes.size() == 1) {
        // Only one essential box left - just convert to polygon
        return Polygon(essential_boxes[0]);
    }
    
    // Check if all boxes can be represented as a single rectangle (common case for dense clusters)
    if (areAllBoxesRectangularlyUnifiable(essential_boxes)) {
        qDebug() << "MaskDataVisualization: Using overall bounding box for rectangular cluster";
        BoundingBox overall_bbox = getOverallBoundingBox(essential_boxes);
        return Polygon(overall_bbox);
    }
    
    // Try to further optimize by checking for simple rectangular unions
    if (essential_boxes.size() == 2 && canUseSimpleRectangularUnion(essential_boxes[0], essential_boxes[1])) {
        qDebug() << "MaskDataVisualization: Using simple rectangular union";
        BoundingBox union_bbox = getSimpleRectangularUnion(essential_boxes[0], essential_boxes[1]);
        return Polygon(union_bbox);
    }
    
    // Fall back to polygon union for complex cases
    qDebug() << "MaskDataVisualization: Using polygon union for complex case with" << essential_boxes.size() << "boxes";
    
    Polygon union_polygon(essential_boxes[0]);
    
    for (size_t i = 1; i < essential_boxes.size(); ++i) {
        Polygon bbox_polygon(essential_boxes[i]);
        union_polygon = union_polygon.unionWith(bbox_polygon);
        
        if (!union_polygon.isValid()) {
            qDebug() << "MaskDataVisualization: Union operation failed at box" << i << ", using bounding box approximation";
            // Fall back to overall bounding box if union fails
            return Polygon(getOverallBoundingBox(essential_boxes));
        }
    }
    
    qDebug() << "MaskDataVisualization: Final union polygon has" << union_polygon.vertexCount() << "vertices";
    return union_polygon;
}

std::vector<float> MaskDataVisualization::generatePolygonVertexData(Polygon const & polygon) const {
    std::vector<float> vertex_data;
    
    if (!polygon.isValid()) {
        return vertex_data;
    }
    
    const auto& vertices = polygon.getVertices();
    vertex_data.reserve(vertices.size() * 2);
    
    for (const auto& vertex : vertices) {
        vertex_data.push_back(vertex.x);
        vertex_data.push_back(vertex.y);
    }
    
    return vertex_data;
}

bool MaskDataVisualization::canUseSimpleRectangularUnion(BoundingBox const & bbox1, BoundingBox const & bbox2) const {
    // Check if they share an edge or are aligned for simple rectangular union
    // This is true if they are:
    // 1. Horizontally aligned (same Y range) and touching/overlapping on X axis
    // 2. Vertically aligned (same X range) and touching/overlapping on Y axis
    // 3. One contains the other (already handled by isCompletelyContained)
    
    const float epsilon = 1e-6f;
    
    // Check horizontal alignment (same Y extents)
    bool horizontally_aligned = (std::abs(bbox1.min_y - bbox2.min_y) < epsilon && 
                               std::abs(bbox1.max_y - bbox2.max_y) < epsilon);
    
    // Check vertical alignment (same X extents)  
    bool vertically_aligned = (std::abs(bbox1.min_x - bbox2.min_x) < epsilon && 
                             std::abs(bbox1.max_x - bbox2.max_x) < epsilon);
    
    if (horizontally_aligned) {
        // Check if they touch or overlap on X axis
        return (bbox1.max_x >= bbox2.min_x - epsilon && bbox1.min_x <= bbox2.max_x + epsilon);
    }
    
    if (vertically_aligned) {
        // Check if they touch or overlap on Y axis
        return (bbox1.max_y >= bbox2.min_y - epsilon && bbox1.min_y <= bbox2.max_y + epsilon);
    }
    
    return false;
}

BoundingBox MaskDataVisualization::getSimpleRectangularUnion(BoundingBox const & bbox1, BoundingBox const & bbox2) const {
    return BoundingBox(
        std::min(bbox1.min_x, bbox2.min_x),
        std::min(bbox1.min_y, bbox2.min_y),
        std::max(bbox1.max_x, bbox2.max_x),
        std::max(bbox1.max_y, bbox2.max_y)
    );
}

BoundingBox MaskDataVisualization::getOverallBoundingBox(std::vector<BoundingBox> const & boxes) const {
    if (boxes.empty()) {
        return BoundingBox(0, 0, 0, 0);
    }
    
    BoundingBox overall = boxes[0];
    for (size_t i = 1; i < boxes.size(); ++i) {
        overall = getSimpleRectangularUnion(overall, boxes[i]);
    }
    
    return overall;
}

bool MaskDataVisualization::areAllBoxesRectangularlyUnifiable(std::vector<BoundingBox> const & boxes) const {
    if (boxes.size() <= 2) {
        return true; // Small number of boxes can always be handled efficiently
    }
    
    // Calculate the overall bounding box
    BoundingBox overall = getOverallBoundingBox(boxes);
    
    // Calculate total area of individual boxes
    float total_individual_area = 0.0f;
    for (const auto& box : boxes) {
        total_individual_area += box.width() * box.height();
    }
    
    // Calculate area of overall bounding box
    float overall_area = overall.width() * overall.height();
    
    // If the overall bounding box area is not much larger than the sum of individual areas,
    // then using the overall bounding box is efficient (boxes are densely packed)
    const float efficiency_threshold = 1.5f; // Allow 50% overhead
    bool is_efficient = (overall_area <= total_individual_area * efficiency_threshold);
    
    if (is_efficient) {
        qDebug() << "MaskDataVisualization: Boxes are densely packed (efficiency ratio:" 
                 << (overall_area / total_individual_area) << ")";
    }
    
    return is_efficient;
}
