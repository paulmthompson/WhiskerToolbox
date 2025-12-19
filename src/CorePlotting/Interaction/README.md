

┌─────────────────────────────────────────────────────────────────────────────┐
│                              Widget Layer                                    │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │  OpenGLWidget                                                           ││
│  │  • Mode state (Normal, CreateInterval, CreateLine, etc.)                ││
│  │  • Routes mouse events to active controller                             ││
│  │  • On complete: receives DataCoordinates, updates DataManager           ││
│  └─────────────────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                    ┌───────────────┴───────────────┐
                    ▼                               ▼
┌───────────────────────────────────┐  ┌──────────────────────────────────────┐
│  CorePlotting::Interaction        │  │  CorePlotting::RenderableScene       │
│  ┌─────────────────────────────┐  │  │  ┌────────────────────────────────┐  │
│  │ IGlyphInteractionController │  │  │  │ Existing batches               │  │
│  │ • Works in canvas coords    │  │  │  │ • poly_line_batches            │  │
│  │ • Tracks geometric state    │  │  │  │ • rectangle_batches            │  │
│  │ • Produces GlyphPreview     │──┼──▶  │ • glyph_batches                │  │
│  └─────────────────────────────┘  │  │  ├────────────────────────────────┤  │
│  ┌─────────────────────────────┐  │  │  │ NEW: Preview layer             │  │
│  │ Concrete Controllers:       │  │  │  │ • preview_rectangles           │  │
│  │ • RectangleController       │  │  │  │ • preview_lines                │  │
│  │ • LineController            │  │  │  │ • preview_polygons             │  │
│  │ • PointController           │  │  │  └────────────────────────────────┘  │
│  │ • PolygonController         │  │  │  ┌────────────────────────────────┐  │
│  └─────────────────────────────┘  │  │  │ Inverse Coordinate API         │  │
└───────────────────────────────────┘  │  │ • canvasToWorld(x, y)          │  │
                                       │  │ • worldToDataCoords(world, key)│  │
                                       │  └────────────────────────────────┘  │
                                       └──────────────────────────────────────┘
                                                        │
                                                        ▼
                                       ┌──────────────────────────────────────┐
                                       │  PlottingOpenGL::SceneRenderer       │
                                       │  • Renders main batches              │
                                       │  • Renders preview layer on top      │
                                       └──────────────────────────────────────┘

Key Types
In CorePlotting/Interaction

// GlyphPreview.hpp - Geometry in canvas coordinates for rendering
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

In CorePlotting/SceneGraph

// Additions to RenderableScene
struct RenderableScene {
    // ... existing batches ...
    
    // Preview layer (rendered on top, canvas coordinates)
    std::optional<GlyphPreview> active_preview;
    
    // Inverse coordinate transform API
    // Requires view/projection to already be set
    [[nodiscard]] glm::vec2 canvasToWorld(float canvas_x, float canvas_y, 
                                          int viewport_width, int viewport_height) const;
    
    // For series-specific transforms (needs series layout info)
    struct DataCoordinates {
        std::string series_key;
        std::optional<EntityId> entity_id;
        
        // Type-specific data coordinates
        struct IntervalCoords { int64_t start; int64_t end; };
        struct LineCoords { float x1, y1, x2, y2; };
        struct PointCoords { float x, y; };
        struct RectCoords { float x, y, w, h; };
        
        std::variant<IntervalCoords, LineCoords, PointCoords, RectCoords> coords;
    };
    
    // Convert preview geometry to data coordinates for the target series
    [[nodiscard]] DataCoordinates previewToDataCoords(
        GlyphPreview const& preview,
        int viewport_width, int viewport_height,
        /* series layout info for Y transform */) const;
};

In PlottingOpenGL

// PreviewRenderer.hpp - Simple renderer for preview geometry
class PreviewRenderer {
public:
    bool initialize();
    void cleanup();
    [[nodiscard]] bool isInitialized() const;
    
    // Renders preview in canvas coordinates (uses screen-space projection)
    void render(GlyphPreview const& preview, int viewport_width, int viewport_height);
    
private:
    // Uses simple 2D shaders for screen-space rendering
    QOpenGLShaderProgram* m_rect_shader{nullptr};
    QOpenGLShaderProgram* m_line_shader{nullptr};
    // ... VAO/VBO for each primitive type ...
};

// SceneRenderer additions
class SceneRenderer {
    // ... existing ...
    
    // Renders preview layer after main scene
    void renderPreview(GlyphPreview const& preview, int viewport_width, int viewport_height);
    
private:
    PreviewRenderer m_preview_renderer;
};

Usage Flow Example (Creating New Interval)

// In OpenGLWidget

// 1. User enters "create interval" mode (toolbar button, hotkey, etc.)
void OpenGLWidget::setInteractionMode(InteractionMode mode) {
    _interaction_mode = mode;
    if (mode == InteractionMode::CreateInterval) {
        _active_controller = std::make_unique<RectangleInteractionController>();
    }
}

// 2. Mouse press starts the interaction
void OpenGLWidget::mousePressEvent(QMouseEvent* event) {
    if (_interaction_mode == InteractionMode::CreateInterval && 
        event->button() == Qt::LeftButton) {
        
        // Find target series (e.g., first visible interval series)
        std::string target_series = findTargetIntervalSeries();
        
        _active_controller->start(
            event->pos().x(), event->pos().y(),
            target_series,
            std::nullopt  // Creating new, not modifying
        );
        update();
    }
}

// 3. Mouse move updates the preview
void OpenGLWidget::mouseMoveEvent(QMouseEvent* event) {
    if (_active_controller && _active_controller->isActive()) {
        _active_controller->update(event->pos().x(), event->pos().y());
        update();
    }
}

// 4. Paint includes the preview
void OpenGLWidget::paintGL() {
    // ... render main scene ...
    _scene_renderer->render();
    
    // Render preview if active
    if (_active_controller && _active_controller->isActive()) {
        _scene_renderer->renderPreview(
            _active_controller->getPreview(),
            width(), height()
        );
    }
}

// 5. Mouse release completes and commits to data
void OpenGLWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (_active_controller && _active_controller->isActive()) {
        _active_controller->complete();
        
        // Convert preview to data coordinates
        auto data_coords = _cached_scene.previewToDataCoords(
            _active_controller->getPreview(),
            width(), height(),
            _cached_layout_response.findLayout(_active_controller->getSeriesKey())
        );
        
        // Commit to DataManager
        commitInteractionResult(data_coords);
        
        _active_controller.reset();
        _interaction_mode = InteractionMode::Normal;
        update();
    }
}