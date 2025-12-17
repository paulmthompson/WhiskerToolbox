#include "SceneRenderer.hpp"

namespace PlottingOpenGL {

SceneRenderer::SceneRenderer() = default;

SceneRenderer::~SceneRenderer() {
    cleanup();
}

bool SceneRenderer::initialize() {
    if (m_initialized) {
        return true;
    }

    // Initialize all batch renderers
    if (!m_poly_line_renderer.initialize()) {
        return false;
    }

    if (!m_glyph_renderer.initialize()) {
        m_poly_line_renderer.cleanup();
        return false;
    }

    if (!m_rectangle_renderer.initialize()) {
        m_glyph_renderer.cleanup();
        m_poly_line_renderer.cleanup();
        return false;
    }

    m_initialized = true;
    return true;
}

void SceneRenderer::cleanup() {
    m_rectangle_renderer.cleanup();
    m_glyph_renderer.cleanup();
    m_poly_line_renderer.cleanup();
    m_initialized = false;
}

bool SceneRenderer::isInitialized() const {
    return m_initialized;
}

void SceneRenderer::uploadScene(CorePlotting::RenderableScene const& scene) {
    // Store scene matrices
    m_view_matrix = scene.view_matrix;
    m_projection_matrix = scene.projection_matrix;

    // Clear previous data
    clearScene();

    // Upload all polyline batches
    for (auto const& batch : scene.poly_line_batches) {
        m_poly_line_renderer.uploadData(batch);
    }

    // Upload all glyph batches
    for (auto const& batch : scene.glyph_batches) {
        m_glyph_renderer.uploadData(batch);
    }

    // Upload all rectangle batches
    for (auto const& batch : scene.rectangle_batches) {
        m_rectangle_renderer.uploadData(batch);
    }
}

void SceneRenderer::clearScene() {
    m_poly_line_renderer.clearData();
    m_glyph_renderer.clearData();
    m_rectangle_renderer.clearData();
    m_view_matrix = glm::mat4{1.0f};
    m_projection_matrix = glm::mat4{1.0f};
}

void SceneRenderer::render() {
    render(m_view_matrix, m_projection_matrix);
}

void SceneRenderer::render(glm::mat4 const& view_matrix, 
                           glm::mat4 const& projection_matrix) {
    if (!m_initialized) {
        return;
    }

    // Render batches in specified order
    for (auto batch_type : m_render_order) {
        switch (batch_type) {
            case BatchType::Rectangle:
                if (m_rectangle_renderer.hasData()) {
                    m_rectangle_renderer.render(view_matrix, projection_matrix);
                }
                break;

            case BatchType::PolyLine:
                if (m_poly_line_renderer.hasData()) {
                    m_poly_line_renderer.render(view_matrix, projection_matrix);
                }
                break;

            case BatchType::Glyph:
                if (m_glyph_renderer.hasData()) {
                    m_glyph_renderer.render(view_matrix, projection_matrix);
                }
                break;
        }
    }
}

void SceneRenderer::setRenderOrder(std::vector<BatchType> const& order) {
    m_render_order = order;
}

} // namespace PlottingOpenGL
