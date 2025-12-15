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

    // Upload polyline batches
    // Note: Current implementation only supports one batch per renderer
    // For multiple batches, we'd need to render them in sequence
    // or combine them into a single batch
    if (!scene.poly_line_batches.empty()) {
        // For now, upload the first batch. 
        // TODO: Support multiple batches by storing a vector in the renderer
        // or rendering multiple batches in sequence
        m_poly_line_renderer.uploadData(scene.poly_line_batches[0]);
    }

    // Upload glyph batches
    if (!scene.glyph_batches.empty()) {
        m_glyph_renderer.uploadData(scene.glyph_batches[0]);
    }

    // Upload rectangle batches
    if (!scene.rectangle_batches.empty()) {
        m_rectangle_renderer.uploadData(scene.rectangle_batches[0]);
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
