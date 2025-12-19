#ifndef GLYPHPREVIEW_HPP
#define GLYPHPREVIEW_HPP

#include "Entity/EntityTypes.hpp"

#include <glm/glm.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

struct GlyphPreview {
    enum class Type { None, Point, Line, Rectangle, Polygon };
    Type type{Type::None};
    
    // Canvas-space geometry
    glm::vec2 point{0};
    glm::vec2 line_start{0}, line_end{0};
    glm::vec4 rectangle{0};  // {x, y, width, height} in canvas
    std::vector<glm::vec2> polygon_vertices;
    
    // Optional "ghost" for modification (original position)
    std::optional<glm::vec4> original_rectangle;
    std::optional<std::pair<glm::vec2, glm::vec2>> original_line;
    
    // Styling
    glm::vec4 fill_color{1, 1, 1, 0.3f};
    glm::vec4 stroke_color{1, 1, 1, 1.0f};
    float stroke_width{2.0f};
};

// IGlyphInteractionController.hpp - Canvas-coordinate controller
class IGlyphInteractionController {
public:
    virtual ~IGlyphInteractionController() = default;
    
    // All coordinates in canvas pixels
    virtual void start(float canvas_x, float canvas_y,
                      std::string series_key,
                      std::optional<EntityId> existing = std::nullopt) = 0;
    virtual void update(float canvas_x, float canvas_y) = 0;
    virtual void complete() = 0;
    virtual void cancel() = 0;
    
    [[nodiscard]] virtual bool isActive() const = 0;
    [[nodiscard]] virtual GlyphPreview getPreview() const = 0;
    [[nodiscard]] virtual std::string const& getSeriesKey() const = 0;
    [[nodiscard]] virtual std::optional<EntityId> getEntityId() const = 0;
};

#endif // GLYPHPREVIEW_HPP