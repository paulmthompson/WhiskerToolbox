#ifndef MEDIA_TOOL_ID_HPP
#define MEDIA_TOOL_ID_HPP

/**
 * @file MediaToolId.hpp
 * @brief Identifiers for Media Viewer toolbar tools
 */

#include <QMetaType>

/**
 * @brief Active tool modes for the Media Viewer left toolbar
 */
enum class MediaToolId {
    None,  ///< No global tool active (canvas pan/zoom and per-datatype modes only)
    Select,///< Pointer / selection tool
    Pen,   ///< Selected-line node editing (append / delete vertices)
    Eraser ///< Brush-style eraser preview (hover circle only in current phase)
};

Q_DECLARE_METATYPE(MediaToolId)

#endif// MEDIA_TOOL_ID_HPP
