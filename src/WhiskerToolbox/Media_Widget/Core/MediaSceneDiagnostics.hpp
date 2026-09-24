/**
 * @file MediaSceneDiagnostics.hpp
 * @brief Lightweight diagnostic data structs for the Media Widget developer panel
 *
 * Returned by Media_Window::getSceneDiagnostics() to expose internal scene item
 * counts without leaking QGraphicsScene implementation details.
 */

#ifndef MEDIA_SCENE_DIAGNOSTICS_HPP
#define MEDIA_SCENE_DIAGNOSTICS_HPP

#include <cstddef>

/**
 * @brief Snapshot of scene element counts in Media_Window
 *
 * Captures the number of QGraphicsScene items in each internal vector at the
 * time getSceneDiagnostics() is called (typically after UpdateCanvas()).
 */
struct MediaSceneDiagnostics {
    std::size_t line_paths = 0;
    std::size_t masks = 0;
    std::size_t mask_bounding_boxes = 0;
    std::size_t mask_outlines = 0;
    std::size_t points = 0;
    std::size_t intervals = 0;
    std::size_t tensors = 0;
    std::size_t text_items = 0;
    std::size_t total_scene_items = 0;///< QGraphicsScene::items().size()
};

#endif// MEDIA_SCENE_DIAGNOSTICS_HPP
